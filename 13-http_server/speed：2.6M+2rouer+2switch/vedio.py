#!/usr/bin/python

import os
import sys
import glob

from mininet.topo import Topo
from mininet.node import OVSBridge
from mininet.net import Mininet
from mininet.cli import CLI
from mininet.link import TCLink

script_deps = [ 'ethtool', 'arptables', 'iptables' ]

def check_scripts():
    dir = os.path.abspath(os.path.dirname(sys.argv[0]))

    script_dir = dir + '/scripts'
    if not os.path.exists(script_dir) or os.path.isfile(script_dir):
        print 'dir "%s" does not exist.' % (script_dir)
        sys.exit(1)
    
    for fname in glob.glob(script_dir + '/*.sh'):
        if not os.access(fname, os.X_OK):
            print '%s should be set executable by using `chmod +x $script_name`' % (fname)
            sys.exit(2)

    for program in script_deps:
        found = False
        for path in os.environ['PATH'].split(os.pathsep):
            exe_file = os.path.join(path, program)
            if os.path.isfile(exe_file) and os.access(exe_file, os.X_OK):
                found = True
                break
        if not found:
            print '`%s` is required but missing, which could be installed via `apt` or `aptitude`' % (program)
            sys.exit(3)

class TCPTopo(Topo):
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        r1 = self.addHost('r1')
        r2 = self.addHost('r2')
        s1 = self.addSwitch('s1')
        s2 = self.addSwitch('s2')

        self.addLink(h1, s1, delay='10ms', loss=2)
        self.addLink(s1, r1)
        self.addLink(h2, s2)
        self.addLink(s2, r2)
        # self.addLink(s1, h2)
        self.addLink(r1, r2)


if __name__ == '__main__':
    check_scripts()

    topo = TCPTopo()
    net = Mininet(topo = topo, switch = OVSBridge, controller = None, link = TCLink) 


    h1, h2, r1, r2, s1,s2= net.get('h1', 'h2', 'r1', 'r2', 's1', 's2')
    h1.cmd('ifconfig h1-eth0 10.0.1.11/24')
    h2.cmd('ifconfig h2-eth0 10.0.2.22/24')

    h1.cmd('route add default gw 10.0.1.1')
    h2.cmd('route add default gw 10.0.2.1')


    r1.cmd('ifconfig r1-eth0 10.0.1.1/24')
    r1.cmd('ifconfig r1-eth1 10.0.3.1/24')
    r1.cmd('route add -net 10.0.2.0 netmask 255.255.255.0 gw 10.0.3.2 dev r1-eth1')

    r2.cmd('ifconfig r2-eth0 10.0.2.1/24')
    r2.cmd('ifconfig r2-eth1 10.0.3.2/24')
    r2.cmd('route add -net 10.0.1.0 netmask 255.255.255.0 gw 10.0.3.1 dev r2-eth1')


    # h1, h2, s1 = net.get('h1', 'h2', 's1')
    # h1.cmd('ifconfig h1-eth0 10.0.0.1/24')
    # h2.cmd('ifconfig h2-eth0 10.0.0.2/24')

    s1.cmd('scripts/disable_ipv6.sh')
    s2.cmd('scripts/disable_ipv6.sh')


    for h in (h1, h2):
        h.cmd('scripts/disable_ipv6.sh')
        h.cmd('scripts/disable_offloading.sh')
        h.cmd('scripts/disable_tcp_rst.sh')
        # XXX: If you want to run user-level stack, you should execute 
        # disable_[arp,icmp,ip_forward].sh first. 

    for r in (r1,r2):
        r.cmd('./scripts/disable_arp.sh')
        r.cmd('./scripts/disable_icmp.sh')
        r.cmd('./scripts/disable_ip_forward.sh')
        r.cmd('./scripts/disable_ipv6.sh')

    net.start()
    r1.cmd('./router &')
    r2.cmd('./router &')
    CLI(net)
    net.stop()