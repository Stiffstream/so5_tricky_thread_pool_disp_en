# What is it?

This is a demo of the creation of own thread_pool dispatcher for SObjectizer-5.6.
This dispatcher uses own demands sheduling policy. The full description of this example can be found [here](https://github.com/Stiffstream/sobjectizer/wiki/SO-5.6-Tutorials-TrickyThreadPool-Intro).

There are two examples inside the repository:

* adv_thread_pool_case. Uses the standard adv_thread_pool-dispatcher from SObjectizer;
* tricky_disp_case. Uses own tricky thread_pool-dispatcher.

# How to get and try?

It is necessary to use a C++ compiler with support for C++17 (checked under gcc-7.3 and vc++ 15.9).

CMake or Ruby+MxxRu are required.

## Compilation with CMake

The simplest way to use CMake is to download an archive with a name like `so5_tricky_thread_pool_disp_en-*-full.zip` from [Releases](https://github.com/Stiffstream/so5_tricky_thread_pool_disp_en/releases). Then:

```sh
unzip so5_tricky_thread_pool_disp_en-202204221030-full.zip
cd so5_tricky_thread_pool_disp_en/dev
mkdir cmake_build
cd cmake_build
cmake ..
cmake --build . --config Release
```

The complied example should be inside `bin` subfolder.


## Compilation with MxxRu

To compile the examples with MxxRu the Ruby and RubyGems are required (RubyGems is installed with Ruby usually, if not the RubyGems should be installed manually).
To install MxxRu the following command should be run:

```sh
gem install Mxx_ru
```

After the installation of Ruby, RubyGems and MxxRu the examples can be taken from the git-repository:

```sh
hg clone https://github.com/Stiffstream/so5_tricky_thread_pool_disp_en
cd so5_tricky_thread_pool_disp_en
mxxruexternals
cd dev
ruby build.rb
```

Or an archive with a name like `so5_tricky_thread_pool_disp_en-*-full.zip` can be downloaded from [Releases](https://github.com/Stiffstream/so5_tricky_thread_pool_disp_en/releases). Then:

```sh
unzip so5_tricky_thread_pool_disp_en-201909051300-full.zip
cd so5_tricky_thread_pool_disp_en/dev
ruby build.rb
```

The complied example should be inside `target/release` subfolder.

One can also do the following command before the compilation:

```sh
cp local-build.rb.example local-build.rb
```

The content of `local-build.rb` can be edited to reflect the specific needs of a user.
