import { join } from 'path';
import { existsSync } from 'fs';

interface ICimgres {
  resize(img: Buffer, width: number, height: number): Buffer;
}

const bindingPath = join(__dirname, '..', 'build', 'cimgres.node');
if (!existsSync(bindingPath)) {
  throw new Error('Addon binary not found. Ensure it is built correctly.');
}

const cimgres: ICimgres = require(bindingPath) as ICimgres;

export default cimgres;
