# Examples
Here are examples of using LibWebGTK from other languages.

To run the python demo, you'll first need it in the same directory as `libwebembed.lib`.

This is so it can find the web process binary.

```bash
# assume your cmake build directory is called _build
cp example.py ../_build/src

# go to directory
cd ../_build/src

# set env variables
# gobject introspection
export GI_TYPELIB_PATH=`pwd`

# serenity resources
export SERENITY_SOURCE_DIR=`pwd`/../../serenity

# run
python3 example.py
```