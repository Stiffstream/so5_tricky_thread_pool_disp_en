[TOC]

# Что это?
Это демонстрация создания собственного thread_pool-диспетчера для SObjectizer-а,
который использует собственную политику распределения заявок между
рабочими нитями в пуле. Данный код служит иллюстрацией для статьи
на Хабрхабр: ["Пишем собственный хитрый thread_pool-диспетчер для SObjectizer-а"](https://habrahabr.ru/post/353712/).

Внутри репозитория два имитационных примера:

* adv_thread_pool_case. Использует штатный adv_thread_pool-диспетчер из состава SObjectizer-а;
* tricky_disp_case. Использует собственный хитрый thread_pool-диспетчер.

# Как взять и попробовать?
Для экспериментов потребуется С++ компилятор с поддержкой C++17 (проверялось
на gcc-7.3 и vc++ 15.6).

Для компиляции потребуется Ruby и MxxRu. Если кому-то нужен вариант, который
можно скомпилировать посредством CMake, то оформите, пожалуйста, issue
в соответствующем [разделе](https://bitbucket.org/sobjectizerteam/so5_tricky_thread_pool_disp_ru/issues).
## Компиляция с помощью MxxRu
Для компиляции с помощью MxxRu потребуется Ruby и RubyGems (как правило, RubyGems устанавливается вместе с Ruby, но если это не так, то RubyGems нужно поставить явно). Для установки MxxRu нужно выполнить команду:

~~~~~{.sh}
gem install Mxx_ru
~~~~~

После того как Ruby, RubyGems и MxxRu установлены можно брать примеры непосредственно из Hg-репозитория:

~~~~~{.sh}
hg clone https://bitbucket.org/sobjectizerteam/so5_tricky_thread_pool_disp_ru
cd so5_tricky_thread_pool_disp_ru
mxxruexternals
cd dev
ruby build.rb
~~~~~

Либо же можно загрузить архив с именем вида so5_tricky_thread_pool_disp_ru-*-full.zip из секции [Downloads](https://bitbucket.org/sobjectizerteam/so5_tricky_thread_pool_disp_ru/downloads/). После чего:

~~~~~{.sh}
unzip so5_tricky_thread_pool_disp_ru-201804181100-full.zip
cd so5_tricky_thread_pool_disp_ru/dev
ruby build.rb
~~~~~

Скомпилированные примеры должны оказаться внутри подкаталога `target/release`.

Также перед сборкой может быть полезно выполнить:

~~~~~{.sh}
cp local-build.rb.example local-build.rb
~~~~~

и нужным вам образом отредактировать содержимое `local-build.rb`.

