# DevDays F2026 - #104 unaligned-ptr-cast-check

[Ссылка на презентацию](https://docs.google.com/presentation/d/1vfzCb6PcbjtUhGF7Vk3Ig3CqOhUGiwXpLJ8cmvQXmNo)

[Ссылка на видео](https://drive.google.com/file/d/1O3ZvcVlaxiAMHSAYOOJSRuRGnOTFKxjF/view?usp=drive_link)

## Описание проекта

Проект представляет собой разработку диагностического модуля MisalignedPointerCastCheck для статического анализатора исходного кода Clang-Tidy, предназначенного для выявления потенциально небезопасных преобразований типов указателей, которые могут привести к нарушению требований выравнивания памяти (misaligned access).

Основная задача модуля заключается в анализе операций приведения типов указателей и определении того, гарантирует ли исходный указатель достаточное выравнивание памяти для целевого типа. Если гарантированное выравнивание исходного адреса оказывается меньше требуемого выравнивания целевого типа, анализатор формирует диагностическое предупреждение о возможном некорректном доступе к памяти.

Для определения гарантированного выравнивания реализован алгоритм статического анализа выражений абстрактного синтаксического дерева (AST) Clang. Алгоритм учитывает различные способы получения указателей и изменения адресов, в том числе:
- Обращение к переменным-указателям и массивам с учётом характеристик их типов.
- Получение адреса переменной или элемента массива.
- Выполнение арифметических операций над указателями, включая сложение и вычитание смещений.
- Преобразование указателей в целочисленные типы и последующее выполнение арифметических операций над полученными значениями.
- Использование константных индексов и смещений для вычисления точного изменения адреса.
- Получение указателей в результате вызовов стандартных функций динамического выделения памяти malloc, calloc и realloc.

## Сборка

```bash
  cmake -S llvm -B build   -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra"   -DCMAKE_BUILD_TYPE=Debug   -DLLVM_TARGETS_TO_BUILD="X86"   -G Ninja

  ninja -C build clang-tidy
```

## Запуск на примере

```bash
   ./build/bin/clang-tidy path/file.cpp -checks='-*,bugprone-misaligned-pointer-cast'   -- -std=c++17
```
или для анализа всего проекта (необходимо наличие compile_commands.json в build исследуемого проекта)

```bash
   python3 ~/path/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py -clang-tidy-binary ~/path/build/bin/clang-tidy -p build -checks='-*,bugprone-misaligned-pointer-cast' -j"$(nproc)"
```

Ожидаемый вывод:

```bash
/PcapPlusPlus/3rdParty/LightPcapNg/LightPcapNg/src/light_internal.c:144:38: warning: casting from type 'uint32_t' (aka 'unsigned int') (guaranteed alignment 4) to type 'struct _light_section_header' (required alignment 8 bytes) may result in misaligned access [bugprone-misaligned-pointer-cast]
  144 |         struct _light_section_header *shb = (struct _light_section_header *)section->block_body;
      |                                             ^
/PcapPlusPlus/3rdParty/LightPcapNg/LightPcapNg/src/light_internal.c:150:4: warning: casting from type 'struct _light_section_header' (guaranteed alignment 4) to type 'struct _light_section_header' (required alignment 8 bytes) may result in misaligned access [bugprone-misaligned-pointer-cast]
  150 |                         shb->section_length = size;
      |                         ^
/PcapPlusPlus/3rdParty/LightPcapNg/LightPcapNg/src/light_internal.c:159:2: warning: casting from type 'struct _light_section_header' (guaranteed alignment 4) to type 'struct _light_section_header' (required alignment 8 bytes) may result in misaligned access [bugprone-misaligned-pointer-cast]
  159 |         shb->section_length = size;
      |         ^
                 ^
```


