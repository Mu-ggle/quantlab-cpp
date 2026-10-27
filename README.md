# QuantLab: C++17 и автоматизация инженерного цикла

[![CI](https://github.com/Mu-ggle/quantlab-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/Mu-ggle/quantlab-cpp/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)

Учебная библиотека для анализа финансовых временных рядов: простые
доходности, скользящее среднее, выборочная годовая волатильность и
максимальная просадка. Демо использует фиксированный небольшой ряд цен,
поэтому не требует API-ключей, рыночных данных или внешних сервисов.

**[Документация Doxygen](https://Mu-ggle.github.io/quantlab-cpp/)** ·
**[Запуски CI и результаты benchmark](https://github.com/Mu-ggle/quantlab-cpp/actions/workflows/ci.yml)** ·
**[中文说明与答辩指南](README.zh-CN.md)**

## Структура

```text
include/quantlab/       Публичный API с комментариями Doxygen
src/                   Реализация библиотеки и консольное демо
tests/                 Unit-тесты doctest
benchmarks/            Измерения Google Benchmark
docs/                  Страница документации и шаблон Doxygen
.github/workflows/     Автоматическая сборка, проверки и публикация
scripts/               Публикация через GitHub CLI
CMakeLists.txt         Конфигурация сборки
AGENTS.md              Назначение, команды и ограничения ИИ-агента
```

## Быстрый старт

Требуются CMake 3.22 или новее, компилятор C++17 и доступ в интернет при первой
конфигурации для загрузки закрепленных версий тестовых зависимостей.
Для документации дополнительно требуется Doxygen.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Запуск демо и benchmark на Linux или с одноконфигурационным генератором:

```sh
./build/quantlab_demo
./build/benchmarks/quantlab_benchmark --benchmark_min_time=0.05s
```

В Windows с Visual Studio используйте:

```powershell
.\build\Release\quantlab_demo.exe
.\build\benchmarks\Release\quantlab_benchmark.exe --benchmark_min_time=0.05s
```

Для MinGW пути: `build/quantlab_demo.exe` и
`build/benchmarks/quantlab_benchmark.exe`. В многоконфигурационных
генераторах `--config Release` выбирает оптимизированную сборку; с Make
или Ninja ее задает `-DCMAKE_BUILD_TYPE=Release` при конфигурации.

Если в Windows используется MinGW, задайте генератор явно в новом каталоге
сборки; иначе CMake может выбрать установленный Visual Studio:

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel
ctest --test-dir build-mingw --output-on-failure
```

Некоторые старые версии MinGW или CMake некорректно обрабатывают пути с
кириллицей или китайскими символами. В таком окружении используйте каталог
из ASCII-символов, например `C:\dev\quantlab-cpp`, и новый каталог сборки.
Не меняйте генератор внутри уже настроенного каталога `build`.

Сборка только библиотеки и демо без тестовых зависимостей:

```sh
cmake -S . -B build-minimal -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DQUANTLAB_BUILD_BENCHMARKS=OFF
cmake --build build-minimal --config Release --parallel
```

## API и численные соглашения

```cpp
#include <quantlab/analytics.hpp>

const std::vector<double> prices{100.0, 110.0, 99.0};
const auto returns = quantlab::simple_returns(prices);   // {0.1, -0.1}
const auto sma = quantlab::moving_average(prices, 2);     // {105, 104.5}
const double drawdown = quantlab::maximum_drawdown(prices); // 0.1 = 10%
const double annual_vol = quantlab::sample_volatility(returns, 252.0);
```

| Операция | Определение и ограничения |
| :--- | :--- |
| Доходности | `p[i+1] / p[i] - 1`; минимум две положительные конечные цены |
| Скользящее среднее | Среднее полного окна; `1 <= window <= values.size()`; результат содержит `n-window+1` значений |
| Волатильность | Выборочное стандартное отклонение с делителем `n-1`, умноженное на `sqrt(periods_per_year)`; минимум две конечные доходности |
| Максимальная просадка | Наибольшее падение относительно уже достигнутого пика; положительные конечные цены; непустой вход |

`periods_per_year` по умолчанию равен 252 и должен быть конечным
положительным числом. Для данных другой частоты параметр задается явно.
NaN, бесконечности и другие нарушения контрактов отклоняются через
`std::invalid_argument`. Непредставимый конечным `double` результат
приводит к `std::overflow_error`.

Все алгоритмы работают за `O(n)`. Для скользящего среднего используется
обновляемая компенсированная сумма окна; для дисперсии - метод Уэлфорда
на центрированных и масштабированных данных. Центрирование уменьшает
потерю точности при большом общем смещении, а масштабирование снижает риск
переполнения промежуточных квадратов. Подробные контракты, примеры и
справочник API доступны в документации Doxygen.

## Тесты и измерения

Unit-тесты используют **doctest**. Они проверяют
известные ответы, граничные случаи, численную устойчивость и обработку
некорректного входа. CTest регистрирует и запускает тестовый executable.

**Google Benchmark** измеряет все четыре функции на детерминированных
массивах нескольких размеров. Входные данные создаются до измеряемого
цикла; результаты защищены от удаления оптимизатором. Время создания
возвращаемых векторов входит в измерение. Команда с фильтром:

```sh
./build/benchmarks/quantlab_benchmark --benchmark_filter=MovingAverage --benchmark_min_time=0.05s
```

Смотрите реальные значения времени в журнале конкретного запуска CI.
Времена зависят от оборудования, компилятора и нагрузки; измерения на
разных CI runners нельзя считать строгим сравнением производительности.

## Документация и CI/CD

Workflow [`ci.yml`](.github/workflows/ci.yml) запускается при push,
pull request и вручную. Он собирает проект, запускает CTest и benchmark,
показывает измеренные времена в журнале и генерирует Doxygen HTML.
Публикация в GitHub Pages выполняется из ветки `main` после успешного
завершения проверок. После публикации badge выше показывает статус
workflow в удаленном репозитории.

Первичная настройка репозитория: **Settings → Pages → Build and
deployment → Source → GitHub Actions**. Сам репозиторий должен быть
публичным. Ссылка на опубликованную документацию:
<https://Mu-ggle.github.io/quantlab-cpp/>.

Локальная генерация:

```sh
cmake -S . -B build -DQUANTLAB_BUILD_DOCS=ON
cmake --build build --target docs
```

Результат: `build/docs/html/index.html`. Предупреждения Doxygen считаются
ошибками и останавливают проверку документации.

### Первичная публикация

После установки GitHub CLI войдите в свой аккаунт и запустите скрипт
из корня проекта в PowerShell 7:

```powershell
gh auth login --hostname github.com --git-protocol https --web --scopes workflow
pwsh -File scripts/publish.ps1 -Owner YOUR_LOGIN
```

Замените `YOUR_LOGIN` своим GitHub login. Скрипт предназначен для создания
публичного репозитория `quantlab-cpp`, подстановки владельца в ссылки,
включения Pages, отправки проекта и проверки удаленного CI. До выполнения
скрипта ссылки на репозиторий и сайт являются шаблонами. Успех подтверждается только
завершившимся workflow и доступным сайтом документации.

## Соответствие критериям задания

| Баллы | Критерий | Где проверять |
| :---: | :--- | :--- |
| 2 | Публичный репозиторий, структура, описание и сборка | Этот README; `src`, `tests`, `benchmarks` |
| 4 | CMake и успешная автоматическая сборка | `CMakeLists.txt`; журнал CI |
| 4 | Unit-тесты на C++ и их успешный запуск в CI | `tests`; шаг CTest |
| 4 | Benchmark и автоматическое измерение в CI | `benchmarks`; журнал с таблицей времени |
| 4 | Doxygen и автоматическая публикация | `docs/Doxyfile.in`; Pages и шаг развертывания |
| 2 | Конфигурация ИИ-агента | `AGENTS.md` в корне |

Всего: **20 баллов по рубрике**. Для полного зачета пунктов 2-5 требуется
успешный удаленный CI, а для документации - доступный опубликованный сайт.
Наличие конфигурационных файлов само по себе это не подтверждает.

## Использование ИИ

Проект создан с помощью ИИ в соответствии с условием домашнего задания.
`AGENTS.md` описывает назначение проекта, команды сборки, тестирования и
измерений, стиль кода и ограничения для последующих изменений. Перед
сдачей нужно понимать формулы и уметь показать успешный CI и сайт
документации; китайский файл содержит краткий разбор для подготовки.
