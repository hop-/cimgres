#include <napi.h>
#include <vips/vips8>
#include <glib.h>
#include <string>

enum class OptionType
{
  NONE,
  RESIZE,
  SCALE,
  SCALE_PERCENT,
};

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
    const int &width, const int &height,
    const char *outFormat = nullptr)
{
  const auto image = vips::VImage::new_from_buffer(data, dataLength, "");

  const auto scale = static_cast<double>(width) / image.width();
  // TODO: use height as well
  const auto resizedImage = image.resize(scale);

  std::string format;
  if (outFormat != nullptr && outFormat[0] != '\0')
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

void scaleImage(
    const uint8_t *data, const size_t &dataLength,
    uint8_t **resizedData, size_t *resizedDataLength,
    const double &scale,
    const char *outFormat = nullptr)
{
  const auto image = vips::VImage::new_from_buffer(data, dataLength, "");

  const auto resizedImage = image.resize(scale);

  std::string format;
  if (outFormat != nullptr && outFormat[0] != '\0')
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

void resize(
    const OptionType &optionType,
    const uint8_t *data, const size_t &dataLength,
    uint8_t **resizedData, size_t *resizedDataLength,
    const int &width, const int &height,
    const double &scale,
    const int &percent,
    const std::string &format)
{
  switch (optionType)
  {
  case OptionType::RESIZE:
    resizeImage(
        data, dataLength,
        resizedData, resizedDataLength,
        width, height,
        format.c_str());
    break;
  case OptionType::SCALE:
    scaleImage(
        data, dataLength,
        resizedData, resizedDataLength,
        scale,
        format.c_str());
    break;
  case OptionType::SCALE_PERCENT:
    scaleImage(
        data, dataLength,
        resizedData, resizedDataLength,
        static_cast<double>(percent) / 100.0,
        format.c_str());
    break;
  default:
    break;
  }
}

bool parseArgs(
    const Napi::CallbackInfo &info,
    OptionType &optionType,
    Napi::Buffer<uint8_t> &buffer,
    int &width, int &height,
    double &scale,
    int &percent,
    std::string &format)
{
  if (info.Length() < 2 || !info[0].IsBuffer() || !info[1].IsObject())
  {
    return false;
  }

  // Get the buffer
  buffer = info[0].As<Napi::Buffer<uint8_t>>();

  // Get the options
  Napi::Object options = info[1].As<Napi::Object>();

  // Check of options type
  if (options.Has("width") && options.Get("width").IsNumber() && options.Has("height") && options.Get("height").IsNumber())
  {
    // Resize
    optionType = OptionType::RESIZE;
    width = options.Get("width").As<Napi::Number>().Int32Value();
    height = options.Get("height").As<Napi::Number>().Int32Value();
  }
  else if (options.Has("scale") && options.Get("scale").IsNumber())
  {
    // Scale
    optionType = OptionType::SCALE;
    scale = options.Get("scale").As<Napi::Number>().DoubleValue();
  }
  else if (options.Has("percent") && options.Get("percent").IsNumber())
  {
    // Scale percent
    optionType = OptionType::SCALE_PERCENT;
    percent = options.Get("percent").As<Napi::Number>().Int32Value();
  }
  else
  {
    return false;
  }

  if (options.Has("format") && options.Get("format").IsString())
  {
    // Format optional parameter
    format = options.Get("format").As<Napi::String>().Utf8Value();
  }

  return true;
}

// Async worker class
class ResizeWorker : public Napi::AsyncWorker
{
private:
  const uint8_t *data;
  const size_t dataLength;
  uint8_t *resizedData;
  size_t resizedDataLength;
  const OptionType optionType;
  const int width;
  const int height;
  const double scale;
  const int percent;
  const std::string format;
  Napi::Promise::Deferred promiseDeferred;

public:
  ResizeWorker(
      Napi::Env &env,
      OptionType &OptionType,
      Napi::Buffer<uint8_t> &buffer,
      int &width, int &height,
      double &scale,
      int &percent,
      std::string &format,
      Napi::Promise::Deferred &promiseDeferred)
      : Napi::AsyncWorker(env),
        data(buffer.Data()),
        dataLength(buffer.Length()),
        resizedData(nullptr),
        resizedDataLength(0),
        optionType(OptionType),
        width(width),
        height(height),
        scale(scale),
        percent(percent),
        format(format),
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
      resize(
          optionType,
          data, dataLength,
          &resizedData, &resizedDataLength,
          width, height,
          scale, percent,
          format);
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

  Napi::Buffer<uint8_t> buffer;
  OptionType optionType = OptionType::NONE;
  int width, height, percent;
  double scale;
  std::string format;

  if (!parseArgs(info, optionType, buffer, width, height, scale, percent, format))
  {
    Napi::TypeError::New(env, "Expected (img: Buffer, options: IOptions)")
        .ThrowAsJavaScriptException();

    return env.Null();
  }

  uint8_t *resizedImageBuffer = nullptr;
  size_t resizedImageBufferLength = 0;
  Napi::Value out;

  try
  {
    resize(
        optionType,
        buffer.Data(), buffer.Length(),
        &resizedImageBuffer, &resizedImageBufferLength,
        width, height,
        scale, percent,
        format);

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

  Napi::Buffer<uint8_t> buffer;
  OptionType optionType = OptionType::NONE;
  int width, height, percent;
  double scale;
  std::string format;

  if (!parseArgs(info, optionType, buffer, width, height, scale, percent, format))
  {
    Napi::TypeError::New(env, "Expected (img: Buffer, options: IOptions)")
        .ThrowAsJavaScriptException();

    return env.Null();
  }

  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  ResizeWorker *worker = new ResizeWorker(env, optionType, buffer, width, height, scale, percent, format, deferred);
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
