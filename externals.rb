MxxRu::arch_externals :so5 do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/sobjectizer/downloads/so-5.6.0.zip'

  e.map_dir 'dev/so_5' => 'dev'
end

MxxRu::arch_externals :clara do |e|
  e.url 'https://github.com/catchorg/Clara/archive/v1.1.5.zip'

  e.map_file 'single_include/clara.hpp' => 'dev/clara/*'
end

MxxRu::arch_externals :fmt do |e|
  e.url 'https://github.com/fmtlib/fmt/archive/5.3.0.zip'

  e.map_dir 'include' => 'dev/fmt'
  e.map_dir 'src' => 'dev/fmt'
  e.map_dir 'support' => 'dev/fmt'
  e.map_file 'CMakeLists.txt' => 'dev/fmt/*'
  e.map_file 'README.rst' => 'dev/fmt/*'
  e.map_file 'ChangeLog.rst' => 'dev/fmt/*'
end

MxxRu::arch_externals :fmtlib_mxxru do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/fmtlib_mxxru/get/fmt-5.0.0.zip'

  e.map_dir 'dev/fmt_mxxru' => 'dev'
end
