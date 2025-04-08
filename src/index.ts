import { join } from 'path';
import { existsSync } from 'fs';

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

const bindingPath = join(__dirname, '..', 'build', 'cimgres.node');
if (!existsSync(bindingPath)) {
  throw new Error('Addon binary not found. Ensure it is built correctly.');
}

const cimgres: ICimgres = require(bindingPath) as ICimgres;

export default cimgres;
