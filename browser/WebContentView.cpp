/*
 * Copyright (c) 2022-2023, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WebContentView.h"
#include "HelperProcess.h"
#include "Utilities.h"
#include <AK/Assertions.h>
#include <AK/ByteBuffer.h>
#include <AK/Format.h>
#include <AK/HashTable.h>
#include <AK/LexicalPath.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/StringBuilder.h>
#include <AK/Types.h>
#include <Kernel/API/KeyCode.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibCore/Timer.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibGfx/ImageFormats/PNGWriter.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Palette.h>
#include <LibGfx/Rect.h>
#include <LibGfx/SystemTheme.h>
#include <LibMain/Main.h>
#include <LibWeb/Crypto/Crypto.h>
#include <LibWeb/Loader/ContentFilter.h>
#include <LibWebView/WebContentClient.h>
#include <cstring>
#include <gdkmm/general.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/memorytexture.h>

#define WEB_GDK_BUTTON_FORWARD 9
#define WEB_GDK_BUTTON_BACKWARD 8

bool is_using_dark_system_theme(Gtk::Widget&);

WebContentView::WebContentView(StringView webdriver_content_ipc_path, WebView::EnableCallgrindProfiling enable_callgrind_profiling, WebView::UseJavaScriptBytecode use_javascript_bytecode)
    : Glib::ObjectBase(typeid(WebContentView))
    , Gtk::Scrollable()
    , Gtk::Widget()
    , m_webdriver_content_ipc_path(webdriver_content_ipc_path)
{
    set_focusable(true);
    set_can_focus(true);

    m_device_pixel_ratio = (float) get_scale_factor();
    m_inverse_pixel_scaling_ratio = 1.0f / m_device_pixel_ratio;

    property_vadjustment().signal_changed().connect([&]() {
        m_vertical_adj = property_vadjustment().get_value();
        m_vertical_adj->property_value().signal_changed().connect([&]() {
            update_viewport_rect();
        });
    });

    property_hadjustment().signal_changed().connect([&]() {
        m_horizontal_adj = property_hadjustment().get_value();
        m_horizontal_adj->property_value().signal_changed().connect([&]() {
            update_viewport_rect();
        });
    });

    signal_map().connect(sigc::mem_fun(*this, &WebContentView::show_event));
    signal_unmap().connect(sigc::mem_fun(*this, &WebContentView::hide_event));

    // Event Controllers
    m_motion_controller = Gtk::EventControllerMotion::create();
    m_motion_controller->signal_motion().connect(sigc::mem_fun(*this, &WebContentView::on_motion));
    add_controller(m_motion_controller);

    m_key_controller = Gtk::EventControllerKey::create();
    m_key_controller->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
    m_key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &WebContentView::on_key_pressed), false);
    m_key_controller->signal_key_released().connect(sigc::mem_fun(*this, &WebContentView::on_key_released), false);
    add_controller(m_key_controller);

    m_click_gesture = Gtk::GestureClick::create();
    m_click_gesture->signal_pressed().connect(sigc::mem_fun(*this, &WebContentView::on_pressed), false);
    m_click_gesture->signal_pressed().connect(sigc::mem_fun(*this, &WebContentView::on_release), false);
    add_controller(m_click_gesture);

    create_client(enable_callgrind_profiling, use_javascript_bytecode);
}

WebContentView::~WebContentView() = default;

unsigned translate_button(unsigned int button)
{
    if (button == GDK_BUTTON_PRIMARY)
        return 1;
    if (button == GDK_BUTTON_SECONDARY)
        return 2;
    if (button == GDK_BUTTON_MIDDLE)
        return 4;
    if (button == WEB_GDK_BUTTON_FORWARD)
        return 8;
    if (button == WEB_GDK_BUTTON_BACKWARD)
        return 16;
    return 0;
}

unsigned translate_buttons(Gdk::ModifierType const state)
{
    auto raw_state = (unsigned) state;

    unsigned buttons = 0;
    if (raw_state & (unsigned) Gdk::ModifierType::BUTTON1_MASK)
        buttons |= 1;
    if (raw_state & (unsigned) Gdk::ModifierType::BUTTON2_MASK)
        buttons |= 2;
    if (raw_state & (unsigned) Gdk::ModifierType::BUTTON3_MASK)
        buttons |= 4;
    // TODO: Are these the forward and backwards buttons?
//    if (raw_state & (unsigned) Gdk::ModifierType::BUTTON4_MASK)
//        buttons |= 8;
//    if (raw_state & (unsigned) Gdk::ModifierType::BUTTON5_MASK)
//        buttons |= 16;
    return buttons;
}

unsigned translate_modifiers(Gdk::ModifierType const state)
{
    auto raw_state = (unsigned) state;

    unsigned modifiers = 0;
    if (raw_state & (unsigned) Gdk::ModifierType::ALT_MASK)
        modifiers |= KeyModifier::Mod_Alt;
    if (raw_state & (unsigned) Gdk::ModifierType::CONTROL_MASK)
        modifiers |= KeyModifier::Mod_Ctrl;
    if (raw_state & (unsigned) Gdk::ModifierType::SHIFT_MASK)
        modifiers |= KeyModifier::Mod_Shift;
    if (raw_state & (unsigned) Gdk::ModifierType::META_MASK)
        modifiers |= KeyModifier::Mod_Super;

    return modifiers;
}

KeyCode translate_keyval(unsigned int keyval)
{
    struct Mapping {
        constexpr Mapping(int q, KeyCode s)
            : gdk_key(q)
            , serenity_key(s)
        {
        }

        int gdk_key;
        KeyCode serenity_key;
    };

    // TODO: These might be a bit broken
    constexpr Mapping mappings[] = {
        { GDK_KEY_0, Key_0 },
        { GDK_KEY_1, Key_1 },
        { GDK_KEY_2, Key_2 },
        { GDK_KEY_3, Key_3 },
        { GDK_KEY_4, Key_4 },
        { GDK_KEY_5, Key_5 },
        { GDK_KEY_6, Key_6 },
        { GDK_KEY_7, Key_7 },
        { GDK_KEY_8, Key_8 },
        { GDK_KEY_9, Key_9 },
        { GDK_KEY_a, Key_A },
        { GDK_KEY_Alt_L, Key_Alt },
        { GDK_KEY_ampersand, Key_Ampersand },
        { GDK_KEY_apostrophe, Key_Apostrophe },
        { GDK_KEY_dead_circumflex, Key_Circumflex },
        { GDK_KEY_asciitilde, Key_Tilde },
        { GDK_KEY_asterisk, Key_Asterisk },
        { GDK_KEY_at, Key_AtSign },
        { GDK_KEY_b, Key_B },
        { GDK_KEY_backslash, Key_Backslash },
        { GDK_KEY_BackSpace, Key_Backspace },
        { GDK_KEY_bar, Key_Pipe },
        { GDK_KEY_braceleft, Key_LeftBrace },
        { GDK_KEY_braceright, Key_RightBrace },
        { GDK_KEY_bracketleft, Key_LeftBracket },
        { GDK_KEY_bracketright, Key_RightBracket },
        { GDK_KEY_c, Key_C },
        { GDK_KEY_Caps_Lock, Key_CapsLock },
        { GDK_KEY_colon, Key_Colon },
        { GDK_KEY_comma, Key_Comma },
        { GDK_KEY_Control_L, Key_Control },
        { GDK_KEY_d, Key_D },
        { GDK_KEY_Delete, Key_Delete },
        { GDK_KEY_dollar, Key_Dollar },
        { GDK_KEY_Down, Key_Down },
        { GDK_KEY_e, Key_E },
        { GDK_KEY_End, Key_End },
        { GDK_KEY_equal, Key_Equal },
        { GDK_KEY_Escape, Key_Escape },
        { GDK_KEY_exclamdown, Key_ExclamationPoint },
        { GDK_KEY_f, Key_F },
        { GDK_KEY_F1, Key_F1 },
        { GDK_KEY_F10, Key_F10 },
        { GDK_KEY_F11, Key_F11 },
        { GDK_KEY_F12, Key_F12 },
        { GDK_KEY_F2, Key_F2 },
        { GDK_KEY_F3, Key_F3 },
        { GDK_KEY_F4, Key_F4 },
        { GDK_KEY_F5, Key_F5 },
        { GDK_KEY_F6, Key_F6 },
        { GDK_KEY_F7, Key_F7 },
        { GDK_KEY_F8, Key_F8 },
        { GDK_KEY_F9, Key_F9 },
        { GDK_KEY_g, Key_G },
        { GDK_KEY_greater, Key_GreaterThan },
        { GDK_KEY_h, Key_H },
        { GDK_KEY_Home, Key_Home },
        { GDK_KEY_i, Key_I },
        { GDK_KEY_Insert, Key_Insert },
        { GDK_KEY_j, Key_J },
        { GDK_KEY_k, Key_K },
        { GDK_KEY_l, Key_L },
        { GDK_KEY_Left, Key_Left },
        { GDK_KEY_less, Key_LessThan },
        { GDK_KEY_m, Key_M },
        { GDK_KEY_Menu, Key_Menu },
        { GDK_KEY_minus, Key_Minus },
        { GDK_KEY_n, Key_N },
        { GDK_KEY_Num_Lock, Key_NumLock },
        { GDK_KEY_o, Key_O },
        { GDK_KEY_p, Key_P },
        { GDK_KEY_Page_Down, Key_PageDown },
        { GDK_KEY_Page_Up, Key_PageUp },
        { GDK_KEY_parenleft, Key_LeftParen },
        { GDK_KEY_parenright, Key_RightParen },
        { GDK_KEY_percent, Key_Percent },
        { GDK_KEY_period, Key_Period },
        { GDK_KEY_plus, Key_Plus },
        { GDK_KEY_Print, Key_PrintScreen },
        { GDK_KEY_q, Key_Q },
        { GDK_KEY_question, Key_QuestionMark },
        { GDK_KEY_quotedbl, Key_DoubleQuote },
        { GDK_KEY_r, Key_R },
        { GDK_KEY_Return, Key_Return },
        { GDK_KEY_Right, Key_Right },
        { GDK_KEY_s, Key_S },
        { GDK_KEY_Scroll_Lock, Key_ScrollLock },
        { GDK_KEY_semicolon, Key_Semicolon },
        { GDK_KEY_Shift_L, Key_LeftShift },
        { GDK_KEY_slash, Key_Slash },
        { GDK_KEY_space, Key_Space },
        { GDK_KEY_Super_L, Key_Super },
        { GDK_KEY_Sys_Req, Key_SysRq },
        { GDK_KEY_t, Key_T },
        { GDK_KEY_Tab, Key_Tab },
        { GDK_KEY_u, Key_U },
        { GDK_KEY_underscore, Key_Underscore },
        { GDK_KEY_Up, Key_Up },
        { GDK_KEY_v, Key_V },
        { GDK_KEY_w, Key_W },
        { GDK_KEY_x, Key_X },
        { GDK_KEY_y, Key_Y },
        { GDK_KEY_z, Key_Z },
    };

    for (auto const& mapping : mappings) {
        if ((int) keyval == mapping.gdk_key)
            return mapping.serenity_key;
    }
    dbgln("Keyval {} is invalid", keyval);
    return Key_Invalid;
}

bool WebContentView::on_key_pressed(guint keyval, guint, Gdk::ModifierType state)
{
//    switch (keyval) {
//        case GDK_KEY_Left:
//        case GDK_KEY_Right:
//        case GDK_KEY_Up:
//        case GDK_KEY_Down:
//        case GDK_KEY_Page_Up:
//        case GDK_KEY_Page_Down:
//            // QAbstractScrollArea::keyPressEvent(event);
//            break;
//        default:
//            break;
//    }

//    if (event->key() == GDK_KEY_Backtab) {
//        // NOTE: Qt transforms Shift+Tab into a "Backtab", so we undo that transformation here.
//        client().async_key_down(KeyCode::Key_Tab, Mod_Shift, '\t');
//        return;
//    }

    gunichar point = gdk_keyval_to_unicode(keyval);
    auto key = translate_keyval(keyval);
    auto modifiers = translate_modifiers(state);
    client().async_key_down(key, modifiers, point);

    return true;
}

void WebContentView::on_key_released(guint keyval, guint, Gdk::ModifierType state)
{
    gunichar point = gdk_keyval_to_unicode(keyval);
    auto key = translate_keyval(keyval);
    auto modifiers = translate_modifiers(state);
    client().async_key_up(key, modifiers, point);
}

void WebContentView::on_pressed(int n_press, double x, double y)
{
    Gfx::IntPoint position(x / m_inverse_pixel_scaling_ratio, y / m_inverse_pixel_scaling_ratio);
    auto button = translate_button(m_click_gesture->get_button());
    if (button == 0) {
        // We could not convert Gtk buttons to something that Lagom can
        // recognize - don't even bother propagating this to the web engine
        // as it will not handle it anyway, and it will (currently) assert
        return;
    }

    auto state = m_click_gesture->get_current_event_state();
    auto modifiers = translate_modifiers(state);
    auto buttons = translate_buttons(state);

    if (n_press > 1) {
        client().async_doubleclick(to_content_position(position), button, buttons, modifiers);
    } else {
        client().async_mouse_down(to_content_position(position), button, buttons, modifiers);
    }
}

void WebContentView::on_release(int n_press, double x, double y)
{
    if (n_press > 1)
        return;

    Gfx::IntPoint position(x / m_inverse_pixel_scaling_ratio, y / m_inverse_pixel_scaling_ratio);
    auto button = translate_button(m_click_gesture->get_button());

    if (button == WEB_GDK_BUTTON_BACKWARD) {
        if (on_back_button)
            on_back_button();
    } else if (button == WEB_GDK_BUTTON_FORWARD) {
        if (on_forward_button)
            on_forward_button();
    }

    if (button == 0) {
        // We could not convert Qt buttons to something that Lagom can
        // recognize - don't even bother propagating this to the web engine
        // as it will not handle it anyway, and it will (currently) assert
        return;
    }
    auto state = m_click_gesture->get_current_event_state();
    auto modifiers = translate_modifiers(state);
    auto buttons = translate_buttons(state);
    client().async_mouse_up(to_content_position(position), button, buttons, modifiers);
}

void WebContentView::on_motion(double x, double y)
{
    Gfx::IntPoint position(x / m_inverse_pixel_scaling_ratio, y / m_inverse_pixel_scaling_ratio);

    auto state = m_click_gesture->get_current_event_state();
    auto buttons = translate_buttons(state);
    auto modifiers = translate_modifiers(state);
    client().async_mouse_move(to_content_position(position), 0, buttons, modifiers);
}

/*void WebContentView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void WebContentView::dropEvent(QDropEvent* event)
{
    VERIFY(event->mimeData()->hasUrls());
    emit urls_dropped(event->mimeData()->urls());
    event->acceptProposedAction();
}

void WebContentView::focusInEvent(QFocusEvent*)
{
    client().async_set_has_focus(true);
}

void WebContentView::focusOutEvent(QFocusEvent*)
{
    client().async_set_has_focus(false);
}*/

void WebContentView::resize_event(int, int)
{
    update_viewport_rect();
    handle_resize();
}

// Runs whenever the widget resizes
void WebContentView::size_allocate_vfunc(int width, int height, int)
{
    resize_event(width, height);
}

void WebContentView::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot)
{
    const auto allocation = get_allocation();
    const Gdk::Rectangle rect(0, 0, allocation.get_width(), allocation.get_height());

    snapshot->scale(m_inverse_pixel_scaling_ratio, m_inverse_pixel_scaling_ratio);

    Gfx::Bitmap const* bitmap;
    Gfx::IntSize bitmap_size;

    if (m_client_state.has_usable_bitmap) {
        bitmap = m_client_state.front_bitmap.bitmap.ptr();
        bitmap_size = m_client_state.front_bitmap.last_painted_size;

    } else {
        bitmap = m_backup_bitmap.ptr();
        bitmap_size = m_backup_bitmap_size;
    }

    if (bitmap) {
        auto cr = snapshot->append_cairo({ 0, 0, bitmap_size.width(), bitmap_size.height() });

        size_t size = bitmap->size_in_bytes();
        const uint8_t* src = bitmap->scanline_u8(0);

        // Copy into temporary buffer
        uint8_t* dest = new uint8_t[size];
        std::memcpy(dest, src, size);

        auto cairo_bitmap = Cairo::ImageSurface::create(dest, Cairo::Surface::Format::RGB24, bitmap->width(), bitmap->height(), 4 * bitmap->width());
        cr->set_source(cairo_bitmap, 0, 0);
        cr->paint();

        if (bitmap_size.width() < rect.get_width()) {
            snapshot->append_color(Gdk::RGBA("white"), { bitmap_size.width(), 0, get_width() - bitmap_size.width(), bitmap->height() });
        }
        if (bitmap_size.height() < rect.get_height()) {
            snapshot->append_color(Gdk::RGBA("white"), { 0, bitmap_size.height(), get_width(), get_height() - bitmap_size.height() });
        }

        return;
    }

    snapshot->append_color(Gdk::RGBA("white"), rect);
}

void WebContentView::set_viewport_rect(Gfx::IntRect rect)
{
    m_viewport_rect = rect;
    client().async_set_viewport_rect(rect);
}

void WebContentView::set_window_size(Gfx::IntSize size)
{
    client().async_set_window_size(size);
}

void WebContentView::set_window_position(Gfx::IntPoint position)
{
    client().async_set_window_position(position);
}

void WebContentView::update_viewport_rect()
{
    auto scaled_width = int((float)get_width() / m_inverse_pixel_scaling_ratio);
    auto scaled_height = int((float)get_height() / m_inverse_pixel_scaling_ratio);
    Gfx::IntRect rect(max(0, m_horizontal_adj->get_value()), max(0, m_vertical_adj->get_value()), scaled_width, scaled_height);

    set_viewport_rect(rect);

    request_repaint();
}

void WebContentView::update_zoom()
{
    client().async_set_device_pixels_per_css_pixel(m_device_pixel_ratio * m_zoom_level);
    update_viewport_rect();
    request_repaint();
}

void WebContentView::show_event()
{
    client().async_set_system_visibility_state(true);
}

void WebContentView::hide_event()
{
    client().async_set_system_visibility_state(false);
}

static Core::AnonymousBuffer make_system_theme_from_gtk_palette(Gtk::Widget& widget, WebContentView::PaletteMode mode)
{
    auto style_context = widget.get_style_context();

    auto theme_file = mode == WebContentView::PaletteMode::Default ? "Default"sv : "Dark"sv;
    auto theme = Gfx::load_system_theme(DeprecatedString::formatted("{}/res/themes/{}.ini", s_serenity_resource_root, theme_file)).release_value_but_fixme_should_propagate_errors();
    auto palette_impl = Gfx::PaletteImpl::create_with_anonymous_buffer(theme);
    auto palette = Gfx::Palette(move(palette_impl));

    auto pack_rgba = [&](const Gdk::RGBA& rgba) {
        uint8_t r = static_cast<uint8_t>(rgba.get_red()   * 255);
        uint8_t g = static_cast<uint8_t>(rgba.get_green() * 255);
        uint8_t b = static_cast<uint8_t>(rgba.get_blue()  * 255);
        uint8_t a = static_cast<uint8_t>(rgba.get_alpha() * 255);

        return (a << 24) | (r << 16) | (g << 8) | b;
    };

    auto translate = [&](Gfx::ColorRole gfx_color_role, Gdk::RGBA rgba /*const char *gtk_color_role*/) {
        // Gdk::RGBA rgba;
        // style_context->lookup_color(gtk_color_role, rgba);
        auto new_color = Gfx::Color::from_argb(pack_rgba(rgba));
        palette.set_color(gfx_color_role, new_color);
    };

    translate(Gfx::ColorRole::ThreedHighlight, Gdk::RGBA("lightgrey"));
    translate(Gfx::ColorRole::ThreedShadow1, Gdk::RGBA("grey"));
    translate(Gfx::ColorRole::ThreedShadow2, Gdk::RGBA("darkgrey"));
    translate(Gfx::ColorRole::HoverHighlight, Gdk::RGBA("lightgrey"));
    translate(Gfx::ColorRole::Link, Gdk::RGBA("blue"));
    translate(Gfx::ColorRole::VisitedLink, Gdk::RGBA("purple"));
    translate(Gfx::ColorRole::Button, Gdk::RGBA("lightgrey"));
    translate(Gfx::ColorRole::ButtonText, Gdk::RGBA("black"));
    translate(Gfx::ColorRole::Selection, Gdk::RGBA("lightblue"));
    translate(Gfx::ColorRole::SelectionText, Gdk::RGBA("black"));

    palette.set_flag(Gfx::FlagRole::IsDark, is_using_dark_system_theme(widget));

    return theme;
}

void WebContentView::update_palette(PaletteMode mode)
{
    client().async_update_system_theme(make_system_theme_from_gtk_palette(*this, mode));
}

void WebContentView::create_client(WebView::EnableCallgrindProfiling enable_callgrind_profiling, WebView::UseJavaScriptBytecode use_javascript_bytecode)
{
    m_client_state = {};

    auto candidate_web_content_paths = get_paths_for_helper_process("WebContent"sv).release_value_but_fixme_should_propagate_errors();
    auto new_client = launch_web_content_process(candidate_web_content_paths, enable_callgrind_profiling, WebView::IsLayoutTestMode::No, use_javascript_bytecode).release_value_but_fixme_should_propagate_errors();

    m_client_state.client = new_client;
    m_client_state.client->on_web_content_process_crash = [this] {
        Core::deferred_invoke([this] {
            dbgln("Crash report received from client");
            handle_web_content_process_crash();
        });
    };

    m_client_state.client_handle = Web::Crypto::generate_random_uuid().release_value_but_fixme_should_propagate_errors();
    client().async_set_window_handle(m_client_state.client_handle);

    client().async_set_device_pixels_per_css_pixel(m_device_pixel_ratio);
    update_palette();
    client().async_update_system_fonts(Gfx::FontDatabase::default_font_query(), Gfx::FontDatabase::fixed_width_font_query(), Gfx::FontDatabase::window_title_font_query());

    // TODO: Doesn't map to GTK/Wayland - can we ignore?
//    auto screens = QGuiApplication::screens();
//
//    if (!screens.empty()) {
//        Vector<Gfx::IntRect> screen_rects;
//
//        for (auto const& screen : screens) {
//            auto geometry = screen->geometry();
//
//            screen_rects.append(Gfx::IntRect(geometry.x(), geometry.y(), geometry.width(), geometry.height()));
//        }
//
//        // FIXME: Update the screens again when QGuiApplication::screenAdded/Removed signals are emitted
//
//        // NOTE: The first item in QGuiApplication::screens is always the primary screen.
//        //       This is not specified in the documentation but QGuiApplication::primaryScreen
//        //       always returns the first item in the list if it isn't empty.
//        client().async_update_screen_rects(screen_rects, 0);
//    }

    if (!m_webdriver_content_ipc_path.is_empty())
        client().async_connect_to_webdriver(m_webdriver_content_ipc_path);
}

void WebContentView::notify_server_did_paint(Badge<WebContentClient>, i32 bitmap_id, Gfx::IntSize size)
{
    if (m_client_state.back_bitmap.id == bitmap_id) {
        m_client_state.has_usable_bitmap = true;
        m_client_state.back_bitmap.pending_paints--;
        m_client_state.back_bitmap.last_painted_size = size;
        swap(m_client_state.back_bitmap, m_client_state.front_bitmap);
        // We don't need the backup bitmap anymore, so drop it.
        m_backup_bitmap = nullptr;
        queue_draw();

        if (m_client_state.got_repaint_requests_while_painting) {
            m_client_state.got_repaint_requests_while_painting = false;
            request_repaint();
        }
    }
}

void WebContentView::notify_server_did_invalidate_content_rect(Badge<WebContentClient>, [[maybe_unused]] Gfx::IntRect const& content_rect)
{
    request_repaint();
}

void WebContentView::notify_server_did_change_selection(Badge<WebContentClient>)
{
    request_repaint();
}

void WebContentView::notify_server_did_request_cursor_change(Badge<WebContentClient>, Gfx::StandardCursor cursor)
{
    switch (cursor) {
    case Gfx::StandardCursor::Hidden:
        set_cursor("none");
        break;
    case Gfx::StandardCursor::Arrow:
        set_cursor("default");
        break;
    case Gfx::StandardCursor::Crosshair:
        set_cursor("crosshair");
        break;
    case Gfx::StandardCursor::IBeam:
        set_cursor("text");
        break;
    case Gfx::StandardCursor::ResizeHorizontal:
        set_cursor("col-resize");
        break;
    case Gfx::StandardCursor::ResizeVertical:
        set_cursor("row-resize");
        break;
    case Gfx::StandardCursor::ResizeDiagonalTLBR:
        set_cursor("nwse-resize");
        break;
    case Gfx::StandardCursor::ResizeDiagonalBLTR:
        set_cursor("nesw-resize");
        break;
    case Gfx::StandardCursor::ResizeColumn:
        set_cursor("col-resize");
        break;
    case Gfx::StandardCursor::ResizeRow:
        set_cursor("row-resize");
        break;
    case Gfx::StandardCursor::Hand:
        set_cursor("pointer");
        break;
    case Gfx::StandardCursor::Help:
        set_cursor("help");
        break;
    case Gfx::StandardCursor::Drag:
        set_cursor("grabbing");
        break;
    case Gfx::StandardCursor::DragCopy:
        set_cursor("copy");
        break;
    case Gfx::StandardCursor::Move:
        set_cursor("grabbing");
        break;
    case Gfx::StandardCursor::Wait:
        set_cursor("wait");
        break;
    case Gfx::StandardCursor::Disallowed:
        set_cursor("not-allowed");
        break;
    case Gfx::StandardCursor::Eyedropper:
    case Gfx::StandardCursor::Zoom:
        // FIXME: No corresponding Qt cursors, default to Arrow
    default:
        set_cursor("default");
        break;
    }
}

void WebContentView::notify_server_did_layout(Badge<WebContentClient>, Gfx::IntSize content_size)
{
    m_vertical_adj->set_lower(0);
    m_vertical_adj->set_upper(content_size.height());
    m_vertical_adj->set_page_size(m_viewport_rect.height());
    m_horizontal_adj->set_lower(0);
    m_horizontal_adj->set_upper(content_size.width());
    m_horizontal_adj->set_page_size(m_viewport_rect.width());
}

void WebContentView::notify_server_did_request_scroll(Badge<WebContentClient>, i32 x_delta, i32 y_delta)
{
    m_horizontal_adj->set_value(max(0, m_horizontal_adj->get_value() + x_delta));
    m_vertical_adj->set_value(max(0, m_vertical_adj->get_value() + y_delta));
}

void WebContentView::notify_server_did_request_scroll_to(Badge<WebContentClient>, Gfx::IntPoint scroll_position)
{
    m_horizontal_adj->set_value(scroll_position.x());
    m_vertical_adj->set_value(scroll_position.y());
}

void WebContentView::notify_server_did_request_scroll_into_view(Badge<WebContentClient>, Gfx::IntRect const& rect)
{
    if (m_viewport_rect.contains(rect))
        return;

    if (rect.top() < m_viewport_rect.top())
        m_vertical_adj->set_value(rect.top());
    else if (rect.top() > m_viewport_rect.top() && rect.bottom() > m_viewport_rect.bottom())
        m_vertical_adj->set_value(rect.bottom() - m_viewport_rect.height());
}

void WebContentView::notify_server_did_enter_tooltip_area(Badge<WebContentClient>, Gfx::IntPoint content_position, DeprecatedString const& tooltip)
{
    (void) content_position; // TODO: Use a popover maybe?
    set_tooltip_text(ustring_from_ak_deprecated_string(tooltip));
}

void WebContentView::notify_server_did_leave_tooltip_area(Badge<WebContentClient>)
{
    set_tooltip_text("");
}

void WebContentView::notify_server_did_request_alert(Badge<WebContentClient>, String const& message)
{
    (void) message;
//    m_dialog = new QMessageBox(QMessageBox::Icon::Warning, "browser", qstring_from_ak_string(message), QMessageBox::StandardButton::Ok, this);
//    m_dialog->exec();
//
//    client().async_alert_closed();
//    m_dialog = nullptr;
}

void WebContentView::notify_server_did_request_confirm(Badge<WebContentClient>, String const& message)
{
    (void) message;
//    m_dialog = new QMessageBox(QMessageBox::Icon::Question, "browser", qstring_from_ak_string(message), QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::Cancel, this);
//    auto result = m_dialog->exec();
//
//    client().async_confirm_closed(result == QMessageBox::StandardButton::Ok || result == QDialog::Accepted);
//    m_dialog = nullptr;
}

void WebContentView::notify_server_did_request_prompt(Badge<WebContentClient>, String const& message, String const& default_)
{
    (void) message;
    (void) default_;
//    m_dialog = new QInputDialog(this);
//    auto& dialog = static_cast<QInputDialog&>(*m_dialog);
//
//    dialog.setWindowTitle("browser");
//    dialog.setLabelText(qstring_from_ak_string(message));
//    dialog.setTextValue(qstring_from_ak_string(default_));
//
//    if (dialog.exec() == QDialog::Accepted)
//        client().async_prompt_closed(ak_string_from_qstring(dialog.textValue()).release_value_but_fixme_should_propagate_errors());
//    else
//        client().async_prompt_closed({});
//
//    m_dialog = nullptr;
}

void WebContentView::notify_server_did_request_set_prompt_text(Badge<WebContentClient>, String const& message)
{
    (void) message;
//    if (m_dialog && is<QInputDialog>(*m_dialog))
//        static_cast<QInputDialog&>(*m_dialog).setTextValue(qstring_from_ak_string(message));
}

void WebContentView::notify_server_did_request_accept_dialog(Badge<WebContentClient>)
{
//    if (m_dialog)
//        m_dialog->accept();
}

void WebContentView::notify_server_did_request_dismiss_dialog(Badge<WebContentClient>)
{
//    if (m_dialog)
//        m_dialog->reject();
}

void WebContentView::notify_server_did_request_file(Badge<WebContentClient>, DeprecatedString const& path, i32 request_id)
{
    auto file = Core::File::open(path, Core::File::OpenMode::Read);
    if (file.is_error())
        client().async_handle_file_return(file.error().code(), {}, request_id);
    else
        client().async_handle_file_return(0, IPC::File(*file.value()), request_id);
}

Gfx::IntRect WebContentView::viewport_rect() const
{
    return m_viewport_rect;
}

Gfx::IntPoint WebContentView::to_content_position(Gfx::IntPoint widget_position) const
{
    return widget_position.translated(max(0, m_horizontal_adj->get_value()), max(0, m_vertical_adj->get_value()));
}

Gfx::IntPoint WebContentView::to_widget_position(Gfx::IntPoint content_position) const
{
    return content_position.translated(-(max(0, m_horizontal_adj->get_value())), -(max(0, m_vertical_adj->get_value())));
}

/*bool WebContentView::event(QEvent* event)
{
    // NOTE: We have to implement event() manually as Qt's focus navigation mechanism
    //       eats all the Tab key presses by default.

    if (event->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return true;
    }
    if (event->type() == QEvent::KeyRelease) {
        keyReleaseEvent(static_cast<QKeyEvent*>(event));
        return true;
    }

    if (event->type() == QEvent::PaletteChange) {
        update_palette();
        request_repaint();
        return QAbstractScrollArea::event(event);
    }

    return QAbstractScrollArea::event(event);
}*/

void WebContentView::notify_server_did_finish_handling_input_event(bool event_was_accepted)
{
    // FIXME: Currently browser handles the keyboard shortcuts before passing the event to web content, so
    //        we don't need to do anything here. But we'll need to once we start asking web content first.
    (void)event_was_accepted;
}

ErrorOr<String> WebContentView::dump_layout_tree()
{
    return String::from_deprecated_string(client().dump_layout_tree());
}
