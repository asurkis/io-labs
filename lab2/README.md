# Лабораторная работа 2

**Название:** "Разработка драйверов блочных устройств"

**Цель работы:**  получить знания и навыки разработки драйверов блочных устройств для операционной системы Linux. 

## Описание функциональности драйвера

Драйвер должен создавать виртуальный жесткий диск в оперативной памяти с размером 50 Мбайт. Созданный жесткий диск разбивается на два первичных и один расширенный разделы с размерами  10 Мбайт, 20 Мбайт и 20 Мбайт соответственно. Расширенный раздел должен быть разделен на два логических с размерами  по 10 Мбайт каждый.

## Инструкция по сборке

1. Собрать проект при помощи `make`.
2. `insmod lab2.ko` - загрузка модуля в ядро.
3. `dmesg` - вывод сообщений ядра.
4. `rmmod lab2` - выгрузка модуля.

## Инструкция пользователя

1. `fdisk -l` - просмотр разделов диска.
2. Форматирование разделов диска:
```
mkfs.vfat /dev/mydisk1
mkfs.vfat /dev/mydisk2
mkfs.vfat /dev/mydisk5
mkfs.vfat /dev/mydisk6
```
3. Создание директорий и монтирование разделов диска, где N - номер раздела:
```
mkdir /mnt/myN
mount /dev/mydiskN /mnt/myN
```
4. `dd if=/dev/urandom of=testN.dat count=$[ 9 * 1024 ]` - заполнение тестовых файла случайными данными.
5. Копирование файлов между разделами виртуального и реального жестких дисков:
```
cp test*.dat /mnt/myN
mkdir testback
cp /mnt/myN/test*.dat testback/dN
```
7. `cp /mnt/myN/test*.dat /mnt/myM` - копирование файлов между разделами созданного виртуального диска, где N, M - разделы диска. 
8. Удаление созданных файлов и размонтирование разделов:
```
rm *.dat
rm -r testback
rm /mnt/myN/*.dat
umount /mnt/myN
```

## Примеры использования

`fdisk -l`

```
Device        Boot    Start     End     Sectors   Size  Id  Type
/dev/mydisk1            1      20480     20480    10M   83  Linux
/dev/mydisk2          20481    61440     40960    20M   83  Linux
/dev/mydisk3          61441   102402     40962    20M    5  Extended
/dev/mydisk5          61442    81921     20480    10M   83  Linux
/dev/mydisk6          81923   102402     20480    10M   83  Linux

```
`time cp test*.dat /mnt/my1`

```
real    0m0,013s
user    0m0,000s
sys     0m0,012s
```
`time cp /mnt/my1/test*.dat testback/d1`

```
real    0m0,018s
user    0m0,000s
sys     0m0,018s
```
`time cp /mnt/my1/test*.dat /mnt/my2`

```
real    0m0,012s
user    0m0,000s
sys     0m0,012s
```
`time cp /mnt/my2/test*.dat /mnt/my5`

```
real    0m0,025s
user    0m0,004s
sys     0m0,021s
```
`time cp /mnt/my5/test*.dat /mnt/my6`

```
real    0m0,013s
user    0m0,000s
sys     0m0,013s
```
