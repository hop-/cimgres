{
  "targets": [
    {
      "target_name": "cimgres",
      "sources": ["src/binding.cpp"],
      "cflags_cc": ["-std=c++17", "-fexceptions"],
      "libraries": ["<!(pkg-config --libs vips glib-2.0)"],
      "include_dirs": [
        "<(module_root_dir)/<!(node -p \"require('node-addon-api').include_dir\")",
        " <!(pkg-config --cflags vips glib-2.0)",
      ],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
    },
  ]
}
