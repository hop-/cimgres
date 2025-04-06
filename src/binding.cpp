#include <napi.h>
#include <vips/vips8>
#include <glib.h>
#include <string>

std::string detectFormat(const void *buf, const size_t &len)
{
  const char *loader = vips_foreign_find_load_buffer(buf, len);

  if (!loader)
    return "";

  if (strcmp(loader, "VipsForeignLoadJpegBuffer") == 0)
    return ".jpg";
  if (strcmp(loader, "VipsForeignLoadPngBuffer") == 0)
    return ".png";
  if (strcmp(loader, "VipsForeignLoadWebpBuffer") == 0)
    return ".webp";
  if (strcmp(loader, "VipsForeignLoadTiffBuffer") == 0)
    return ".tif";
  if (strcmp(loader, "VipsForeignLoadHeifBuffer") == 0)
    return ".heif";
  if (strcmp(loader, "VipsForeignLoadJxlBuffer") == 0)
    return ".jxl";
  if (strcmp(loader, "VipsForeignLoadGifBuffer") == 0)
    return ".gif"; // older gif loader
  if (strcmp(loader, "VipsForeignLoadNsgifBuffer") == 0)
    return ".gif"; // newer gif loader
  if (strcmp(loader, "VipsForeignLoadJp2kBuffer") == 0)
    return ".jp2"; // JPEG2000
  if (strcmp(loader, "VipsForeignLoadPdfBuffer") == 0)
    return ".pdf";
  if (strcmp(loader, "VipsForeignLoadSvgBuffer") == 0)
    return ".svg";
  if (strcmp(loader, "VipsForeignLoadRadBuffer") == 0)
    return ".hdr"; // Radiance
  if (strcmp(loader, "VipsForeignLoadMagick7Buffer") == 0)
    return ".magick"; // fallback, could be many formats

  return "";
}

void resizeImage(
    const uint8_t *data, const size_t &dataLength,
    uint8_t **resizedData, size_t *resizedDataLength,
    const int &width, int height,
    const char *outFormat = nullptr)
{
  const auto image = vips::VImage::new_from_buffer(data, dataLength, "");

  const auto scale = static_cast<double>(width) / image.width();
  const auto resizedImage = image.resize(scale);

  std::string format;
  if (outFormat)
  {
    format = outFormat;
  }
  else
  {
    format = detectFormat(data, dataLength);
    if (format.empty())
    {
      throw vips::VError("Unsupported image format");
    }
  }

  resizedImage.write_to_buffer(
      format.c_str(),
      reinterpret_cast<void **>(resizedData),
      resizedDataLength);
}

// Async worker class
class ResizeWorker : public Napi::AsyncWorker
{
private:
  const uint8_t *data;
  const size_t dataLength;
  uint8_t *resizedData;
  size_t resizedDataLength;
  const int width;
  const int height;
  Napi::Promise::Deferred promiseDeferred;

public:
  ResizeWorker(Napi::Env &env, Napi::Buffer<uint8_t> &buffer, int &width, int &height, Napi::Promise::Deferred &promiseDeferred)
      : Napi::AsyncWorker(env),
        data(buffer.Data()),
        dataLength(buffer.Length()),
        resizedData(nullptr),
        resizedDataLength(0),
        width(width),
        height(height),
        promiseDeferred(promiseDeferred)
  {
  }

  ~ResizeWorker()
  {
    if (resizedData == nullptr)
    {
      g_free(resizedData);
      resizedData = nullptr;
      resizedDataLength = 0;
    }
  }

  void Execute() override
  {
    try
    {
      resizeImage(
          data, dataLength,
          &resizedData, &resizedDataLength,
          width, height);
    }
    catch (const vips::VError &e)
    {
      SetError(e.what());
    }
  }

private:
  void OnOK() override
  {
    // Napi::Env env = Env();
    Napi::HandleScope scope(Env());

    Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::Copy(
        Env(),
        resizedData,
        resizedDataLength);

    promiseDeferred.Resolve(buffer);
  }

  void OnError(const Napi::Error &e) override
  {
    promiseDeferred.Reject(e.Value());
  }
};

Napi::Value resizeSync(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() < 3 || !info[0].IsBuffer() || !info[1].IsNumber() || !info[2].IsNumber())
  {
    Napi::TypeError::New(env, "Expected (imageBuffer: Buffer, width: number, height: number)")
        .ThrowAsJavaScriptException();

    return env.Null();
  }

  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
  int width = info[1].As<Napi::Number>().Int32Value();
  int height = info[2].As<Napi::Number>().Int32Value();

  if (width <= 0 || height <= 0)
  {
    Napi::TypeError::New(env, "Width and height must be positive numbers")
        .ThrowAsJavaScriptException();

    return env.Null();
  }

  uint8_t *resizedImageBuffer = nullptr;
  size_t resizedImageBufferLength = 0;
  Napi::Value out;

  try
  {
    resizeImage(
        buffer.Data(), buffer.Length(),
        &resizedImageBuffer, &resizedImageBufferLength,
        width, height);

    out = Napi::Buffer<uint8_t>::Copy(
        env,
        resizedImageBuffer,
        resizedImageBufferLength);
  }
  catch (const vips::VError &e)
  {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
  }

  g_free(resizedImageBuffer);
  return out;
}

Napi::Value resizeAsync(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (info.Length() < 3 ||
      !info[0].IsBuffer() ||
      !info[1].IsNumber() ||
      !info[2].IsNumber())
  {
    Napi::TypeError::New(env, "Expected (Buffer, width, height)").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Buffer<uint8_t> inputBuffer = info[0].As<Napi::Buffer<uint8_t>>();

  int width = info[1].As<Napi::Number>().Int32Value();
  int height = info[2].As<Napi::Number>().Int32Value();
  if (width <= 0 || height <= 0)
  {
    Napi::TypeError::New(env, "Width and height must be positive numbers")
        .ThrowAsJavaScriptException();

    return env.Null();
  }

  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  ResizeWorker *worker = new ResizeWorker(env, inputBuffer, width, height, deferred);
  worker->Queue();

  return deferred.Promise();
}

Napi::Object init(Napi::Env env, Napi::Object exports)
{
  if (VIPS_INIT("cimgres"))
  {
    Napi::Error::New(env, "Failed to initialize libvips").ThrowAsJavaScriptException();

    return exports;
  }

  exports.Set(Napi::String::New(env, "resizeSync"), Napi::Function::New(env, resizeSync));
  exports.Set(Napi::String::New(env, "resize"), Napi::Function::New(env, resizeAsync));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, init)
