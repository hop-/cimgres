interface IScaleOptions {
  scale: number;
}

interface IScalePercentOptions {
  percent: number;
}

interface ISizeOptions {
  width: number;
  height: number;
}

export type ImageFormat = '.jpg' | '.png' | '.webp' | '.tiff' | '.gif' | '.bmp' | '.svg' | '.pdf'

export interface IFormatOptions {
  format?: ImageFormat;
}

export type IResizeOptions =  IScaleOptions | ISizeOptions | IScalePercentOptions;

export type IOptions = IResizeOptions & IFormatOptions;

interface ICimgres {
  resize(img: Buffer, options: IOptions ): Promise<Buffer>;
  resizeSync(img: Buffer, options: IOptions): Buffer;
}

const cimgres: ICimgres = require('node-gyp-build')(`${__dirname}/..`) as ICimgres;

export default cimgres;
