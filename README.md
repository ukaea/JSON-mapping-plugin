# JSON Mapping Plugin

## Compilation + Contribution (JH + SD)
STILL VERY ROUGH  (#nohaters)  
(AP: Everything has been rewritten so many times, I have kinda lost track.. 
Will be committed to branches now rather than main)

Compile something like this
```bash
#!/bin/bash

export BOOST_HOME=/opt/homebrew/Cellar/boost/1.82.0_1/include

UDA_HOME=/Users/aparker/UDADevelopment/install
export PKG_CONFIG_PATH=$UDA_HOME/lib/pkgconfig:$PKG_CONFIG_PATH

cmake -Bbuild_adam -H. -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$UDA_HOME \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    "$@"
```

Run clang-format (uses sources and headers defined in  
```bash
cmake --build build-adam --target clang-format
```

### TODO:  
- [ ] Add third-party licence information
- [ ] Reimplement exprtk MAP_TYPE
- [ ] Fix simple cache implementation (JH help)
- [ ] Commit documentation for plugins AND mappings
- [ ] Rework argument parsing when imas_plugin has been updated
- [ ] Commit tests + add new tests for changed functionality
- [ ] Separate DRaFT_data_reader from JSON_mapping_plugin
- [ ] Move slice to PLUGIN MAP_TYPE
- [ ] Test with AL5
- [ ] Provide container option for installation
- [ ] Mapping file handler hard coded to 3.38.1 DD version

### MASTU TODO:  
- [ ] Rewrite GEOM plugin
- [ ] Change MAST-U mappings to new types
- [ ] Test AL5 with MAST-U data
- [ ] Generate all IDSs

### Third-party dependencies
Nlohmann JSON - v3.11.2  
License : MIT  
Pantor/Inja - v3.4.0  
License : MIT  
GSL/gsl-lite - v0.41.0  
License : MIT  
Exprtk - (Jan 1st Release: version unlisted)  
License : MIT
