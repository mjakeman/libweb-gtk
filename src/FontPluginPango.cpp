/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023, Linus Groh <linusg@serenityos.org>
 * Copyright (c) 2023, Matthew Jakeman <mattjakemandev@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FontPluginPango.h"
#include <AK/DeprecatedString.h>
#include <AK/String.h>
#include <LibCore/StandardPaths.h>
#include <LibGfx/Font/Emoji.h>
#include <LibGfx/Font/FontDatabase.h>
// #include <QFont>
// #include <QFontInfo>
#include <pangomm.h>

extern DeprecatedString s_serenity_resource_root;

namespace Ladybird {

FontPluginGTK::FontPluginGTK(bool is_layout_test_mode)
    : m_is_layout_test_mode(is_layout_test_mode)
{
    // Load the default SerenityOS fonts...
    Gfx::FontDatabase::set_default_fonts_lookup_path(DeprecatedString::formatted("{}/res/fonts", s_serenity_resource_root));

    // ...and also anything we can find in the system's font directories
    for (auto const& path : Core::StandardPaths::font_directories().release_value_but_fixme_should_propagate_errors())
        Gfx::FontDatabase::the().load_all_fonts_from_path(path.to_deprecated_string());

    Gfx::FontDatabase::set_default_font_query("Katica 10 400 0");
    Gfx::FontDatabase::set_fixed_width_font_query("Csilla 10 400 0");

    Gfx::Emoji::set_emoji_lookup_path(String::formatted("{}/res/emoji", s_serenity_resource_root).release_value_but_fixme_should_propagate_errors());

    update_generic_fonts();

    auto default_font_name = generic_font_name(Web::Platform::GenericFont::UiSansSerif);
    m_default_font = Gfx::FontDatabase::the().get(default_font_name, 12.0, 400, Gfx::FontWidth::Normal, 0);
    VERIFY(m_default_font);

    auto default_fixed_width_font_name = generic_font_name(Web::Platform::GenericFont::UiMonospace);
    m_default_fixed_width_font = Gfx::FontDatabase::the().get(default_fixed_width_font_name, 12.0, 400, Gfx::FontWidth::Normal, 0);
    VERIFY(m_default_fixed_width_font);

//    m_default_font = Gfx::FontDatabase::the().get("Arial", 12.0, 400, Gfx::FontWidth::Normal, 0);
//            VERIFY(m_default_font);
//
//    m_default_fixed_width_font = Gfx::FontDatabase::the().get("Andale Mono", 12.0, 400, Gfx::FontWidth::Normal, 0);
//            VERIFY(m_default_fixed_width_font);
}

FontPluginGTK::~FontPluginGTK() = default;

Gfx::Font& FontPluginGTK::default_font()
{
    return *m_default_font;
}

Gfx::Font& FontPluginGTK::default_fixed_width_font()
{
    return *m_default_fixed_width_font;
}

void FontPluginGTK::update_generic_fonts()
{
    // How we choose which system font to use for each CSS font:
    // 1. Ask Qt via the QFont::StyleHint mechanism for the user's preferred font.
    // 2. Try loading that font through Gfx::FontDatabase
    // 3. If we don't support that font for whatever reason (e.g missing TrueType features in LibGfx)...
    //    1. Try a list of known-suitable fallback fonts with their names hard-coded below
    //    2. If that didn't work, fall back to Gfx::FontDatabase::default_font() (or default_fixed_width_font())

    // This is rather weird, but it's how things work right now, as we can only draw with fonts loaded by LibGfx.

    m_generic_font_names.resize(static_cast<size_t>(Web::Platform::GenericFont::__Count));

    auto update_mapping = [&](Web::Platform::GenericFont generic_font, ReadonlySpan<DeprecatedString> fallbacks) {
        if (m_is_layout_test_mode) {
            m_generic_font_names[static_cast<size_t>(generic_font)] = "SerenitySans";
            return;
        }

        Pango::FontDescription font_desc;

        if (generic_font == Web::Platform::GenericFont::Monospace)
            font_desc.set_family("monospace");
        else if (generic_font == Web::Platform::GenericFont::Fantasy)
            font_desc.set_family("fantasy");
        else if (generic_font == Web::Platform::GenericFont::Cursive)
            font_desc.set_family("cursive");

        auto pango_font_family = font_desc.get_family();

        auto gfx_font = Gfx::FontDatabase::the().get(pango_font_family.c_str(), 16, 400, Gfx::FontWidth::Normal, 0, Gfx::Font::AllowInexactSizeMatch::Yes);
        if (!gfx_font) {
            for (auto& fallback : fallbacks) {
                gfx_font = Gfx::FontDatabase::the().get(fallback, 16, 400, Gfx::FontWidth::Normal, 0, Gfx::Font::AllowInexactSizeMatch::Yes);
                if (gfx_font)
                    break;
            }
        }

        if (!gfx_font) {
            if (generic_font == Web::Platform::GenericFont::Monospace || generic_font == Web::Platform::GenericFont::UiMonospace)
                gfx_font = Gfx::FontDatabase::default_fixed_width_font();
            else
                gfx_font = Gfx::FontDatabase::default_font();
        }

        m_generic_font_names[static_cast<size_t>(generic_font)] = gfx_font->family();
    };

    // Fallback fonts to look for if Gfx::Font can't load the font suggested by Qt.
    // The lists are basically arbitrary, taken from https://www.w3.org/Style/Examples/007/fonts.en.html
    Vector<DeprecatedString> cursive_fallbacks { "Comic Sans MS", "Comic Sans", "Apple Chancery", "Bradley Hand", "Brush Script MT", "Snell Roundhand", "URW Chancery L" };
    Vector<DeprecatedString> fantasy_fallbacks { "Impact", "Luminari", "Chalkduster", "Jazz LET", "Blippo", "Stencil Std", "Marker Felt", "Trattatello" };
    Vector<DeprecatedString> monospace_fallbacks { "Andale Mono", "Courier New", "Courier", "FreeMono", "OCR A Std", "DejaVu Sans Mono", "Liberation Mono", "Csilla" };
    Vector<DeprecatedString> sans_serif_fallbacks { "Arial", "Helvetica", "Verdana", "Trebuchet MS", "Gill Sans", "Noto Sans", "Avantgarde", "Optima", "Arial Narrow", "Liberation Sans", "Katica" };
    Vector<DeprecatedString> serif_fallbacks { "Times", "Times New Roman", "Didot", "Georgia", "Palatino", "Bookman", "New Century Schoolbook", "American Typewriter", "Liberation Serif", "Roman" };

    update_mapping(Web::Platform::GenericFont::Cursive, cursive_fallbacks);
    update_mapping(Web::Platform::GenericFont::Fantasy, fantasy_fallbacks);
    update_mapping(Web::Platform::GenericFont::Monospace, monospace_fallbacks);
    update_mapping(Web::Platform::GenericFont::SansSerif, sans_serif_fallbacks);
    update_mapping(Web::Platform::GenericFont::Serif, serif_fallbacks);
    update_mapping(Web::Platform::GenericFont::UiMonospace, monospace_fallbacks);
    update_mapping(Web::Platform::GenericFont::UiRounded, sans_serif_fallbacks);
    update_mapping(Web::Platform::GenericFont::UiSansSerif, sans_serif_fallbacks);
    update_mapping(Web::Platform::GenericFont::UiSerif, serif_fallbacks);
}

DeprecatedString FontPluginGTK::generic_font_name(Web::Platform::GenericFont generic_font)
{
    return m_generic_font_names[static_cast<size_t>(generic_font)];
}

}
