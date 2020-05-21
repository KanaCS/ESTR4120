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

vmb, vmc: sudo route add default 10.0.36.1 //always check using "route", this guy will disappear autoly

sudo ./nat 10.3.1.36 10.0.36.0 24 100 1 //vma

echo "AAAA" | nc -uv 137.189.88.153 10100 //vmb or vmc

**generate file:**

 base64 /dev/urandom | head -c \[size_in_byte\] > file.txt
 
# TO DO
- [X] Translation expiry requiremen
- [X] Inbound & Outbound
- [ ] ICMP error translation (what actually needed to be done lol cannot understand spec
- [ ] DEBUG Inbound (half done, not yet check delivery if package //dont know how to check QAQ 
- [ ] DEBUG Outbound 
- [ ] Token bucket: hv not use it yet in forwarding of pkt
- [ ] Threads

