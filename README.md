# HRTF Oto Game

立体音響を使って遊びたかった.

参考
[頭部伝達関数を使って8D立体音響を実装](https://nettyukobo.com/3d_sound/)

使用するには西野隆典研究室のホームページにある頭部伝達関数データベースからHRTF data (2)をダウンロードし解凍してください.
http://www.sp.m.is.nagoya-u.ac.jp/HRTF

Requirements
[OpenSiv3d](https://github.com/Siv3D/OpenSiv3D/)

使用方法
```
git clone --recursive https://github.com/yadokani389/HrtfOtoGame
cd HrtfOtoGame
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cd ..
cmake --build build
wget https://drive.usercontent.google.com/download\?id\=1Wnmd120f1lVFJfC4ziCZ_vD-2mrAnM_- -O hrtfs.zip
unzip hrtfs.zip
./HrtfOtoGame
```
