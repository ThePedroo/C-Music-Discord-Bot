sudo apt update
sudo apt -y install curl libsqlite3-dev build-essential wget libcurl4-openssl-dev libssl-dev git clang 

git clone https://github.com/Cogmasters/concord.git -b dev
cd concord
make -j8
sudo make install

cd ..
clang main.c -o ConcordBot -Wall -Wextra -Werror -O2 -ldiscord -lcurl -lsqlite3 -pthread