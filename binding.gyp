{
  "variables": {
    "undefined": "",
    "openssl_fips": "",
  },
  "targets": [
    {
      "target_name": "cimgres",
      "sources": ["src/binding.cpp"],
      "cflags_cc": ["-std=c++17", "-fexceptions"],
      "libraries": ["-Wl,-rpath,/usr/lib/x86_64-linux-gnu", "<!(pkg-config --libs vips-cpp glib-2.0)"],
      "include_dirs": [
        "/usr/local/include/node",
        "<(module_root_dir)/<!(node -p \"require('node-addon-api').include_dir\")",
        " <!(pkg-config --cflags vips glib-2.0)",
      ],
      "ldflags": ["-Wl,-unresolved-symbols=ignore-all"],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "NAPI_VERSION=8",
      ],
      "conditions": [
        [ "OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "NO",
            "MACOSX_DEPLOYMENT_TARGET": "10.10"
          }
        }],
        [ "OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": { "ExceptionHandling": 0 }
          },
          "defines": [ "WIN32_LEAN_AND_MEAN" ]
        }]
      ],
    },
  ]
}
