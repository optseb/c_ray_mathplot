# Compound-ray and mathplot demo program

This is an example program that:

a) uses compound-ray to determine what a user-specified compound eye can see

and:

b) Uses mathplot to render the eye within its environment

This simple repository builds one program, **c_ray_mathplot**, which opens a
glTF-encoded 3D environment containing a compound eye camera and
renders the view using a mathplot VisualModel called
mplot::compoundray::EyeVisual.

Before compiling c_ray_mathplot, obtain and compile NVidia Optix 8.0 and obtain,
compile and `make install` compound-ray from Seb's fork:

https://github.com/optseb/compound-ray

To compile c_ray_mathplot, first clone sebsjames/mathplot, sebsjames/maths into
the base of this repo:

```bash
cd c_ray_mathplot
git clone git@github.com:sebsjames/maths
git clone git@github.com:sebsjames/mathplot
```

then do a cmake build, passing the cmake variable OptiX_INSTALL_DIR
just as you will have done during the compound-ray build process:

```bash
mkdir build
cd build
cmake -DOptiX_INSTALL_DIR=~/src/NVIDIA-OptiX-SDK-8.0.0-linux64-x86_64 ..
make
```

Now you can run the program

```bash
./build/bin/c_ray_mathplot -f ./data/axis_coloured_blocks.gltf
# or
./build/bin/c_ray_mathplot -f ./data/natural_env.gltf
```

This will show the 'hexy' eye looking at a scene containing 6 coloured blocks.

Author: Seb James
Date: September 2025