#include <napi.h>
#include <vips/vips8>
#include <glib.h>
#include <iostream>

// Async worker class
class ResizeWorker : public Napi::AsyncWorker {
private:
  uint8_t* data;
  size_t dataLength;
  uint8_t* resizedData;
  size_t resizedDataLength;
  int width;
  int height;
  Napi::Promise::Deferred promiseDeferred;

public:
  ResizeWorker(Napi::Env env, Napi::Buffer<uint8_t> buffer, int width, int height, Napi::Promise::Deferred promiseDeferred)
    : Napi::AsyncWorker(env),
      data(buffer.Data()),
      dataLength(buffer.Length()),
      width(width),
      height(height),
      promiseDeferred(promiseDeferred)
  {}

  ~ResizeWorker() {}

  void Execute() override
  {
    try {
      vips::VImage image = vips::VImage::new_from_buffer(data, dataLength, "");
      double scale = static_cast<double>(width) / image.width();
      vips::VImage resizedImage = image.resize(scale);

      resizedImage.write_to_buffer(
        ".jpeg",
        reinterpret_cast<void**>(&resizedData),
        &resizedDataLength
      );
    } catch (const vips::VError &e) {
      SetError(e.what());
    }
  }

private:
  void OnOK() override
  {
    // Napi::Env env = Env();
    Napi::HandleScope scope(Env());

    Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::New(
      Env(),
      resizedData,
      resizedDataLength
    );

    promiseDeferred.Resolve(buffer);
  }

  void OnError(const Napi::Error& e) override
  {
    promiseDeferred.Reject(e.Value());
  }
};

Napi::Value resizeSync(const Napi::CallbackInfo& info)
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

    image = image.resize(scale);

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

Napi::Value resizeAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 ||
        !info[0].IsBuffer() ||
        !info[1].IsNumber() ||
        !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected (Buffer, width, height)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Buffer<uint8_t> inputBuffer = info[0].As<Napi::Buffer<uint8_t>>();

    int width = info[1].As<Napi::Number>().Int32Value();
    int height = info[2].As<Napi::Number>().Int32Value();
    if (width <= 0 || height <= 0) {
      Napi::TypeError::New(env, "Width and height must be positive numbers")
        .ThrowAsJavaScriptException();

      return env.Null();
    }


    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    ResizeWorker* worker = new ResizeWorker(env, inputBuffer, width, height, deferred);
    worker->Queue();

    return deferred.Promise();
}


Napi::Object init(Napi::Env env, Napi::Object exports)
{
  if (VIPS_INIT("cimgres")) {
    Napi::Error::New(env, "Failed to initialize libvips").ThrowAsJavaScriptException();

    return exports;
  }

  exports.Set(Napi::String::New(env, "resizeSync"), Napi::Function::New(env, resizeSync));
  exports.Set(Napi::String::New(env, "resize"), Napi::Function::New(env, resizeAsync));


  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, init)
