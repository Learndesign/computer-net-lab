#!/usr/bin/python

from mininet.node import OVSBridge
from mininet.topo import Topo
from mininet.net import Mininet
from mininet.cli import CLI
from time import sleep
import os

# os.system('chmod +x scripts/disable_offloading.sh')
# os.system('chmod +x scripts/disable_ipv6.sh')
# os.system('chmod +x scripts/disable_icmp.sh')
# os.system('chmod +x scripts/disable_ip_forward.sh')
# os.system('chmod +x scripts/disable_arp.sh')

class NATTopo(Topo):
    def build(self):
        s1 = self.addSwitch('s1')
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        h3 = self.addHost('h3')
        n1 = self.addHost('n1')

        self.addLink(h1, s1)
        self.addLink(h2, s1)
        self.addLink(n1, s1)
        self.addLink(h3, n1)

if __name__ == '__main__':
    topo = NATTopo()
    net = Mininet(topo = topo, switch = OVSBridge, controller = None) 

    h1, h2, h3, s1, n1 = net.get('h1', 'h2', 'h3', 's1', 'n1')

    h1.cmd('ifconfig h1-eth0 10.21.0.1/16')
    h1.cmd('route add default gw 10.21.0.254')

    h2.cmd('ifconfig h2-eth0 10.21.0.2/16')
    h2.cmd('route add default gw 10.21.0.254')

    n1.cmd('ifconfig n1-eth0 10.21.0.254/16')
    n1.cmd('ifconfig n1-eth1 159.226.39.43/24')

    h3.cmd('ifconfig h3-eth0 159.226.39.123/24')

    for h in (h1, h2, h3):
        h.cmd('./scripts/disable_offloading.sh')
        h.cmd('./scripts/disable_ipv6.sh')

    s1.cmd('./scripts/disable_ipv6.sh')

    n1.cmd('./scripts/disable_arp.sh')
    n1.cmd('./scripts/disable_icmp.sh')
    n1.cmd('./scripts/disable_ip_forward.sh')
    n1.cmd('./scripts/disable_ipv6.sh')

 
    net.start()
    # test1
    # n1.cmd('./nat exp1.conf > n1-output.txt 2>&1 &' )
    # h3.cmd('python http_server.py > h3-output.txt 2>&1 &')
    
    # for h in (h1, h2):
    #     h.cmd('wget http://159.226.39.123:8000 > %s-output.txt 2>&1' % h)   

    # n1.cmd('pkill -SIGTERM nat')
    
    # test2
    n1.cmd('./nat exp2.conf > n1-output.txt 2>&1 &' )
    
    h1.cmd('python http_server.py > h1-output.txt 2>&1 &')
    h2.cmd('python http_server.py > h2-output.txt 2>&1 &')
    
    h3.cmd('wget http://159.226.39.43:8000 > h3-output.txt 2>&1')   
    h3.cmd('wget http://159.226.39.43:8001 > h3-output.txt 2>&1')   

    n1.cmd('pkill -SIGTERM nat')
    
    # CLI(net)
    net.stop()
