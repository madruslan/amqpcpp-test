# amqpcpp-test
Example to find memory leaks

```bash
mkdir build && cd build
cmake .. -DAMQPCPP_INCLUDE_DIR=/opt/include
make
./amqp
```

to check the memory leaks run the script

```bash
while true; do pid=$(pidof amqp); echo $(ps o rsz -p $pid | tail -n1); sleep 10; done
```
example:

```bash
$ while true; do pid=$(pidof amqp); echo $(ps o rsz -p $pid | tail -n1); sleep 10; done
4932
5196
5724
5988
6252
6780
7044
7308
7572
7836
8100
8628
```
