#include <napi.h>
#include <vips/vips8>

Napi::Value Resize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsString() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected (inputPath: string, width: number, height: number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string inputPath = info[0].As<Napi::String>();
    int width = info[1].As<Napi::Number>().Int32Value();
    int height = info[2].As<Napi::Number>().Int32Value();
    std::string outputPath = "output.jpg"; // Change as needed

    try {
        vips::VImage image = vips::VImage::new_from_file(inputPath.c_str());
        image = image.resize((double)width / image.width(), vips::VImage::option()->set("height", height));
        image.write_to_file(outputPath.c_str());
    } catch (const vips::VError &e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }

    return Napi::String::New(env, outputPath);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    if (VIPS_INIT("cimgres")) {
        Napi::Error::New(env, "Failed to initialize libvips").ThrowAsJavaScriptException();
        return exports;
    }

    exports.Set(Napi::String::New(env, "resize"), Napi::Function::New(env, Resize));
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
