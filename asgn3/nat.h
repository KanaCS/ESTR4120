run:
sudo ./run.sh
make
sudo ./nat

when stop process and re-run
run:
sudo kill <process number>
  (remark: process number can be check by "kill %")

check the iptables rules:
sudo iptables -L