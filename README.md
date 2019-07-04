[TOC]

# What is it?

This is a demo of the creation of own thread_pool dispatcher for SObjectizer-5.6.
This dispatcher uses own demands sheduling policy. The full description of this example can be found [here](https://bitbucket.org/sobjectizerteam/sobjectizer/wiki/so-5.6-docs/tutorials/tricky_thread_pool_disp/intro).

There are two examples inside the repository:

* adv_thread_pool_case. Uses the standard adv_thread_pool-dispatcher from SObjectizer;
* tricky_disp_case. Uses own tricky thread_pool-dispatcher.

# How to get and try?

It is necessary to use a C++ compiler with support for C++17 (checked under gcc-7.3 and vc++ 15.9).

Ruby and MxxRu are required. If someone needs a support for CMake please create an issue.

## Compilation with MxxRu

To compile the examples with MxxRu the Ruby and RubyGems are required (RubyGems is installed with Ruby usually, if not the RubyGems should be installed manually).
To install MxxRu the following command should be run:

~~~~~{.sh}
gem install Mxx_ru
~~~~~

After the installation of Ruby, RubyGems and MxxRu the examples can be taken from the Hg-repository:

~~~~~{.sh}
hg clone https://bitbucket.org/sobjectizerteam/so5_tricky_thread_pool_disp_en
cd so5_tricky_thread_pool_disp_en
mxxruexternals
cd dev
ruby build.rb
~~~~~

Or an archive with a name like so5_tricky_thread_pool_disp_en-*-full.zip can be downloaded from [Downloads](https://bitbucket.org/sobjectizerteam/so5_tricky_thread_pool_disp_en/downloads/). После чего:

~~~~~{.sh}
unzip so5_tricky_thread_pool_disp_en-201907041100-full.zip
cd so5_tricky_thread_pool_disp_en/dev
ruby build.rb
~~~~~

The complied example should be inside `target/release` subfolder.

One can also do the following command before the compilation:

~~~~~{.sh}
cp local-build.rb.example local-build.rb
~~~~~

The content of `local-build.rb` can be edited to reflect the specific needs of a user.
