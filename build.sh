sudo apt update
sudo apt -y install curl libsqlite3-dev build-essential wget libcurl4-openssl-dev libssl-dev git clang postgresql postgresql-server-dev-all make

git clone https://github.com/Cogmasters/concord.git -b dev
cd concord
make -j8
sudo make install

cd ..
make
