
#!/bin/bash

import os

basedir = os.path.dirname(__file__)

rootdir = os.path.join(basedir, '..', '..')
os.system(f'cd {rootdir}; rm -f index.html')
os.system(f'cd {rootdir}; emmake make -f scripts/emscripten/Makefile_emscripten clean')
os.system(f'cd {rootdir}; emmake make -f scripts/emscripten/Makefile_emscripten -j$(nproc)')
print('\n=========================================================================')
print('If the compilation succeed, run the following command in the root directory to test the webpage:')
print(f'$ emrun index.html')

