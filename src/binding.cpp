#include <napi.h>
#include <vips/vips8>
#include <glib.h>

Napi::Value resize(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsBuffer() || !info[1].IsNumber() || !info[2].IsNumber()) {
    Napi::TypeError::New(env, "Expected (imageBuffer: Buffer, width: number, height: number)")
      .ThrowAsJavaScriptException();

    return env.Null();
  }

  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
  int width = info[1].As<Napi::Number>().Int32Value();
  int height = info[2].As<Napi::Number>().Int32Value();

  if (width <= 0 || height <= 0) {
    Napi::TypeError::New(env, "Width and height must be positive numbers")
      .ThrowAsJavaScriptException();

    return env.Null();
  }

  uint8_t* resizedImageBuffer;
  size_t resizedImageBufferLength = 0;

  try {
    vips::VImage image = vips::VImage::new_from_buffer(buffer.Data(), buffer.Length(), NULL);

    const double scale = static_cast<double>(width) / image.width();
    const auto resizeOptions = vips::VImage::option()->set("height", height);

    image = image.resize(scale, resizeOptions);

    image.write_to_buffer(
      ".jpeg",
      reinterpret_cast<void**>(&resizedImageBuffer),
      &resizedImageBufferLength
    );
  } catch (const vips::VError &e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();

    return env.Null();
  }

  return Napi::Buffer<uint8_t>::New(
    env,
    resizedImageBuffer,
    resizedImageBufferLength
  );
}

Napi::Object init(Napi::Env env, Napi::Object exports)
{
  if (VIPS_INIT("cimgres")) {
    Napi::Error::New(env, "Failed to initialize libvips").ThrowAsJavaScriptException();

    return exports;
  }

  exports.Set(Napi::String::New(env, "resize"), Napi::Function::New(env, resize));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, init)
