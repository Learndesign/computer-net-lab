import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  

def read_cwnd(fname):
    fn = fname + '/cwnd.txt'
    with open(fn) as f:
        list = [line.split(', ') for line in f]
    list.pop()
    x0 = float(list[0][0])
    x = [float(line[0])-x0 for line in list]
    y = [int(line[1].split('cwnd:')[1].split(' ')[0]) for line in list]
    return (x,y)

def read_qlen(fname):
    fn = fname + '/qlen.txt'
    with open(fn) as f:
        list = [line for line in f]
    list.pop()
    list = [line.split(', ') for line in list]
    x0 = float(list[0][0])
    x = [float(line[0])-x0 for line in list]
    y = [int(line[1]) for line in list]
    return (x,y)

def read_rtt(fname):
    return read_iperf_rtt(fname)

def atof(str):
    sum = 0.0
    isrem = 0
    first = True
    for i in str:
        if i == ' ' and first:
            continue
        first = False
        add = ord(i) - ord('0')
        if add>=0 and add<=9:
            if isrem==0:
                sum = sum*10 + add
            else:
                sum += add/isrem
                isrem *= 10
        elif i=='.' and isrem==0:
            isrem = 10.0
        else:
            break
    return sum

def read_bw(fname):
    fn = fname + '/iperf.txt'
    x = []
    y = []
    with open(fn) as f:
        nline=0
        for line in f:
            nline += 1
            if nline<7 or nline%2==1:
                continue
            x.append(float(line.split('] ')[1].split('-')[0]))
            y.append(atof(line.split('Bytes  ')[1]))
    return (x,y)

def read_iperf_rtt(fname):
    fn = fname + '/iperf.txt'
    x = []
    y = []
    with open(fn) as f:
        nline=0
        for line in f:
            nline += 1
            if nline<7 or nline%2==1:
                continue
            x.append(float(line.split('] ')[1].split('-')[0]))
            y.append(float(line.split('K/')[1].split(' us')[0])/1000000)
    return (x,y)


maxqlen = [20,40,60,80,100]
colors = ['r','g','b','y','m']

def draw_rep(yname):
    y = []
    for i in range(len(maxqlen)):
        func_name = 'read_'+yname
        y.append(eval(func_name)('qlen-'+str(maxqlen[i])))
    plt.figure(dpi=300)
    for i in range(len(maxqlen)):   
        plt.plot(y[i][0],y[i][1],label='maxqlen=%d'%maxqlen[i],linewidth=1.1,color=colors[i])

    plt.xlim(-5,65)
    #plt.ylim(1,200)
    plt.grid()
    plt.xlabel('time(s)')
    if yname == 'rtt':
        plt.ylabel(yname+'(s)')
    elif yname == 'bw':
        plt.ylabel(yname+'(Mbps)')
    else:
        plt.ylabel(yname+'(packets)')
    plt.legend(loc='upper right')
    plt.savefig(yname+'.png')#,dpi=300)

def draw_mit(namelist):
    y = []
    for i in namelist:
        y.append(read_rtt(i))
    
    plt.figure(figsize=(12,5), dpi=300)
    from brokenaxes import brokenaxes
    bax = brokenaxes(xlims = ((0,300),),ylims=((0, 4), (9, 12)), hspace=.08, despine=False)
    for i in range(len(namelist)):   
        bax.plot(y[i][0],y[i][1],label='algo=%s'%namelist[i],linewidth=1.2,color=colors[i])

    #bax.grid()
    bax.set_xlabel('time(s)')
    bax.set_ylabel('per-packet queue delay for dynamic bandwidth(s)')
    bax.legend(loc='upper left')
    plt.savefig('miti.png')#,dpi=300)
    return

draw_rep('bw')
draw_rep('cwnd')
draw_rep('qlen')
draw_rep('rtt')


algo = ['red','taildrop','codel']
draw_mit(algo)
