# SCGIServer
Данный репозиторий представляет собой реализацию асинхронного, многопоточного сервера с протоколом [SCGI](https://ru.wikipedia.org/wiki/SCGI).

Для установки мониториннга файловых дескрипторов используется библиотека _epoll_. Многопоточность реализуется с помощью библиотеки _threads_.

## Сборка проекта
Для сборки используется _cmake_. Команды сборки:
```bash
cmake -D CMAKE_BUILD_TYPE=Release ..
cmake --build . --target all
```

Подробнее про реализацию, технологии и архитектуру можно прочитать в [файле](https://github.com/NikitaYurasov/SCGIServer/blob/main/docs.pdf).
