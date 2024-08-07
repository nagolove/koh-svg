#!/usr/bin/env bash

#set -Exeuo pipefail
#set -Exeuo pipefail

# Массив с именами файлов
files=(
    "assets/magic_level_01.svg"
    "assets/magic_level_02.svg"
    "assets/magic_level_03.svg"
    "assets/magic_level_04.svg"
    "assets/magic_level_05.svg"
)

# Ваша команда, которую нужно применить к каждому файлу
# Пример команды: your_command input_file output_file
# Замените 'your_command' на вашу конкретную команду

for file in "${files[@]}"; do
  # Изменение расширения файла с .svg на .png
  #output_file=$(basename "${file%.svg}.png")
  output_file="${file%.svg}.png"
  
  # Вызов команды с двумя аргументами: исходный файл и измененный файл
  echo "svglint $file"
  svglint $file
  
  if [ $? -ne 0 ]; then
      echo "Файл $file некорректен. Пропуск."
      continue
  fi

  # экспортровать с расширением видимой зоны на всю геометрию
  inkscape --export-type=png --export-filename=$output_file --export-area-drawing $file 2>/dev/null
done

