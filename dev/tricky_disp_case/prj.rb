require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

  target 'tricky_disp_case_app'

  required_prj 'fmt_mxxru/prj.rb'
  required_prj 'so_5/prj_s.rb'

  cpp_source 'main.cpp'
}
