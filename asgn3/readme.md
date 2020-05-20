**v2 is the one without multithreading and it work well**

**run:**
sudo ./run.sh
make
sudo ./nat

**when stop process and re-run:**
kill % //get the process number 
sudo kill <process number>

**check the iptables rules:**
sudo iptables -L

**testing:** 
sudo ./nat 10.3.1.36 10.0.36.0 24 100 1 //vma
echo "AAAA" | nc -uv 137.189.88.153 10100 //vmb or vmc
