futhark opencl --library render.fut
gcc render.c -o librender.so -fPIC -shared
gcc run.c -o run -L ~/Documents/futhark/render/ -lrender -lOpenCL -lm -lpng
./run
