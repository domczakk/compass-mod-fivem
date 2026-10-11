# fivem-compass-asi

Lokalny kompas HUD dla klienta FiveM, napisany w C++17 jako plugin `compass.asi`.
Inspiracja wyglądem: [JohnDeved/fivem-compass](https://github.com/JohnDeved/fivem-compass).
Implementacja korzysta z natywów ScriptHookV, bez resource, Lua, JavaScript,
`fxmanifest.lua`, eventów sieciowych i zmian w plikach serwera.

## Wymagania i ograniczenia

- Windows x64, klient FiveM oraz serwer dopuszczający pluginy ScriptHookV:
  **`sv_scriptHookAllowed 1`**. To warunek po stronie administratora serwera;
  plugin go nie zmienia ani nie omija. Bez tego ustawienia mod może nie zostać załadowany.
- Do budowania: Visual Studio 2022 z pakietem **Desktop development with C++**,
  Windows SDK i CMake 3.15 lub nowszy.
- Oficjalny [ScriptHookV SDK](https://www.dev-c.com/gtav/scripthookv/).
  Zgodność konkretnych wersji FiveM, gry i interfejsu ScriptHookV wymaga testu w grze.

Wynik to jeden plik `.asi`. Opcja `/MT` (w Debug `/MTd`) dołącza runtime C++
statycznie, więc nie trzeba dostarczać dodatkowej biblioteki MSVC DLL.
`ScriptHookV.lib` jest jednak biblioteką importową, a nie samodzielnym silnikiem
gry: plugin wymaga interfejsu ScriptHookV zapewnionego przez kompatybilnego hosta
FiveM. Nie jest niezależnym programem ani binarium bez importów systemowych.
Nie kopiuj loadera `dinput8.dll` ani dystrybucji ScriptHookV dla single-player do
FiveM w ramach tej instalacji.

## SDK i kompilacja

1. Pobierz **SDK**, nie tylko paczkę uruchomieniową ScriptHookV, z oficjalnej strony.
2. Skopiuj całą zawartość katalogu nagłówków SDK do `sdk/inc/`, a bibliotekę
   x64 `ScriptHookV.lib` do `sdk/lib/`. Zachowaj również zależne nagłówki.
   Kod używa `main.h` i `natives.h`; jeśli paczka zawiera `ScriptHookV.h`,
   skopiuj go razem z pozostałymi plikami. Nie twórz zastępczych nagłówków SDK.
3. Otwórz terminal deweloperski Visual Studio w katalogu tego repozytorium:

   ```powershell
   cmake -B build -A x64
   cmake --build build --config Release
   ```

4. Wynik: `build/Release/compass.asi`.

CMake sprawdza platformę, kompilator, architekturę i obecność podstawowych
plików SDK. Projekt stosuje `/W3`, C++17 i `_CRT_SECURE_NO_WARNINGS`.
SDK i wyniki budowania nie trafiają do Git. Linux nie jest platformą docelową;
nie można tu potwierdzić zgodności ABI za pomocą samego kompilatora GCC.

## Instalacja i użycie

1. Zamknij FiveM.
2. Utwórz katalog `%localappdata%\FiveM\FiveM.app\plugins\`, jeśli nie istnieje.
3. Skopiuj do niego `compass.asi` i uruchom ponownie FiveM.
4. Po uruchomieniu skryptu powstanie `compass.ini` obok pluginu.
5. **F7** przełącza widoczność i zapisuje stan. Przytrzymanie nie powoduje
   wielokrotnych przełączeń; debounce wynosi 250 ms. Klawisz działa, gdy okno
   procesu gry jest na pierwszym planie.

Konfiguracja jest odczytywana na początku `ScriptMain`. Aby zmienić parametry,
zamknij FiveM, edytuj INI i uruchom klienta ponownie. Zapis po przełączeniu
widoczności utrwala wszystkie aktualnie załadowane opcje; nie edytuj INI w trakcie gry.
Katalog pluginu musi umożliwiać zapis. Błąd zapisu nie wyłącza samego kompasu.

## compass.ini

Wszystkie opcje znajdują się w sekcji `[Compass]`.

| Klucz | Domyślnie | Znaczenie / zakres |
| --- | --- | --- |
| `visible` | `1` | Widoczność: 0 = ukryty, wartość niezerowa = widoczny |
| `barWidth` | `0.32` | Szerokość względem ekranu, 0.10–0.90, następnie korekta safe zone i proporcji |
| `yPos` | `0.06` | Środek paska w pionie, 0.04–0.94, w obrębie safe zone |
| `colorR` | `255` | Składowa czerwona tekstu i kresek, 0–255 |
| `colorG` | `255` | Składowa zielona, 0–255 |
| `colorB` | `255` | Składowa niebieska, 0–255 |
| `bgAlpha` | `100` | Przezroczystość czarnego tła, 0–255 |
| `fov` | `120` | Zakres kierunków widoczny na pasku w stopniach, 30–360 |
| `toggleKey` | `118` | Dziesiętny Windows Virtual-Key, 1–254; 118 = `VK_F7` |
| `showDegrees` | `0` | Etykiety liczbowe przy niekardynalnych kreskach co 15°, 0/1 |
| `labels` | `N,E,S,W` | Cztery etykiety CSV w kolejności północ, wschód, południe, zachód |

Liczby zmiennoprzecinkowe zapisuj z kropką. Wartości poza zakresem są
klampowane; nieprawidłowe liczby otrzymują wartość domyślną. Pola całkowite
akceptują wyłącznie zapis dziesiętny (np. `118`, nie `0x76`). Niepoprawny
`toggleKey` przywraca F7. Zapis i odczyt ułamków są niezależne od ustawień
językowych systemu. Zbyt długie, obcięte wartości INI są odrzucane.
CSV musi zawierać dokładnie cztery niepuste etykiety, każda do 8 drukowalnych
znaków ASCII, bez spacji wewnętrznych i `~` (znacznik formatowania GTA).
Spacje otaczające etykiety są usuwane. Niepoprawny CSV pozostawia domyślne etykiety.

```ini
[Compass]
visible=1
barWidth=0.32
yPos=0.06
colorR=255
colorG=255
colorB=255
bgAlpha=100
fov=120
toggleKey=118
showDegrees=0
labels=N,E,S,W
```

## Rysowanie i odporność

Heading pochodzi z `CAM::GET_GAMEPLAY_CAM_ROT(2).z`. Znak yaw jest odwracany,
aby 0° oznaczało N, 90° E, 180° S, 270° W. Gdy odczyt kamery zgłosi wyjątek
lub zwróci NaN/Infinity, używane jest `ENTITY::GET_ENTITY_HEADING` gracza.
Wartość 0 jest prawidłowym kierunkiem, nie sygnałem błędu. Jeśli oba źródła
są niedostępne, klatka kompasu jest pomijana.

Delta kierunku jest zawijana do `[-180, 180)`. Kreski poza FOV są pomijane,
a tekst przy brzegach ma dodatkowy margines. Pozycja i rozmiary korzystają
z `GET_SAFE_ZONE_SIZE`, `GET_ASPECT_RATIO` i środka ekranu 0.5; na szerokich
monitorach szerokość jest korygowana, a grubość kresek uwzględnia proporcje.
Tekst korzysta z natywów `UI::SET_TEXT_*`, `UI::_SET_TEXT_ENTRY`,
`UI::_ADD_TEXT_COMPONENT_STRING`, `UI::_DRAW_TEXT`.
Tło, kreski i znacznik środka używają `GRAPHICS::DRAW_RECT`.
**`DRAW_LINE` przyjmuje współrzędne świata 3D, nie HUD 2D**, dlatego nie nadaje
się do ekranowych kresek i celowo nie jest wywoływany.

Pętla oddaje sterowanie przez `WAIT(0)` po każdej iteracji. Obsługa SEH osłania
wywołania natywów: błąd rysowania wyłącza renderer do restartu, błąd `WAIT`
kończy skrypt, a błędy safe zone/aspect ratio uruchamiają wartości zastępcze.
Komunikaty diagnostyczne trafiają do `OutputDebugString` (debugger Windows).
Ta ochrona nie potrafi naprawić niezgodnego ABI, błędu loadera, przerwania
procesu ani natywu, który bez wyjątku zwraca nieprawidłowe dane. Nie stanowi
gwarancji braku crashy dowolnej wersji hosta.

## Weryfikacja na Windows / FiveM

- Zbuduj Release z prawdziwym SDK; sprawdź brak błędów i wynik `.asi`.
- Na serwerze dopuszczającym ScriptHookV sprawdź pojawienie się HUD i INI.
- Obróć kamerę przez N/E/S/W, również przy nieruchomej postaci; sprawdź płynne
  przejście przez 359°/0°, kreski co 15° i pomijanie elementów poza paskiem.
- Naciśnij oraz przytrzymaj F7; sprawdź pojedyncze przełączenie i zachowanie
  widoczności po restarcie. Sprawdź brak przełączania po utracie fokusu.
- Sprawdź `showDegrees=1`, własne etykiety, wartości poza zakresami,
  rozdzielczości 16:9 i ultrawide oraz różne ustawienia safe zone.

Testy w grze wymagają Windows i działającego klienta FiveM. Sama kontrola
źródeł lub test z atrapami natywów nie potwierdza działania pluginu w grze.

## Testy regresji konfiguracji (bez SDK)

Testy kompilują produkcyjny `src/config.cpp` z testową atrapą funkcji profili
Windows. Działają na Linuksie (GCC/Clang) i Windows (MSVC), bez SDK i gry:

```sh
cmake -S . -B build-tests -DCOMPASS_BUILD_PLUGIN=OFF -DCOMPASS_BUILD_TESTS=ON
cmake --build build-tests --config Release
cmake -E chdir build-tests ctest --output-on-failure -C Release
```

Obejmują wartości domyślne, zapis/odczyt widoczności, ścieżkę INI, błędne
liczby, przepełnienia, klampowanie, CSV, ponowne ładowanie, ułamki przy locale
z przecinkiem i błędy zapisu/odczytu ścieżki. Kontrole działają również w Release.
Atrapa jest dostępna tylko dla celu testowego i nie jest linkowana do `.asi`.
Testy nie sprawdzają rzeczywistego Windows API, ABI ScriptHookV ani renderowania
w grze. Domyślny build nadal tworzy wyłącznie plugin i wymaga Windows/MSVC/SDK.
