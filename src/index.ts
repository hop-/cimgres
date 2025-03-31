import { join } from 'path';
import { existsSync } from 'fs';

interface ICimgres {
  HelloWorld(): string;
}

const bindingPath = join(__dirname, '..', 'build', 'your-addon.node');
if (!existsSync(bindingPath)) {
  throw new Error('Addon binary not found. Ensure it is built correctly.');
}

const cimgres: ICimgres = require(bindingPath);

export default cimgres;
