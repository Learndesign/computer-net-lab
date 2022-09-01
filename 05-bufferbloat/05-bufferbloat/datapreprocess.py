# -*- coding: utf-8 -*-
import os
import re

pwd = os.path.dirname(os.path.abspath(__file__))

project_path = os.path.dirname(os.path.abspath(__file__)) + \
               r'\qlen-'
result_data_path = os.path.dirname(os.path.abspath(__file__)) + \
               r'\data-len-'

basic_lab = ['20', '40', '60', '80', '100']

pro_lab = [r'\taildrop', r'\red', r'\codel']

pro_reault ='\pro'


cwnd = []
time_cwnd = []


def read_cwnd_data():
    for test in basic_lab:
        begin_time = 0.0
        if os.path.isdir(result_data_path + test):
            pass
        else:
            os.mkdir(result_data_path + test)
        
        with open(result_data_path + test + r'\cwnd_data.csv', 'w', encoding='utf8') as w_file: 
            w_file.write('time'+','+'value'+'\n')   
            with open(project_path + test + r'\cwnd.txt', 'r', encoding='utf8') as r_file:
                while True:
                    line = r_file.readline()
                    if not line:
                        break;
                    data = line.split(' ')
                    now = 0.0;
                    if begin_time == 0.0:
                        begin_time = float(data[0][0:-1])
                        now = 0.0
                        time_cwnd.append(0.0)
                    else:
                        now = float(data[0][0:-1]) - begin_time
                        time_cwnd.append(now)
                    cwnd_num = 0
                    for i in data:
                        if i.split(':')[0] == 'cwnd':
                            cwnd_num = int(i.split(':')[1])
                            cwnd.append(cwnd_num)
                            break
                    w_file.write(str(now) + ',' + str(cwnd_num) +'\n')
            r_file.close()     
        w_file.close()    

def read_qlen_data():
    for test in basic_lab:
        begin_time = 0.0
        if os.path.isdir(result_data_path + test):
            pass
        else:
            os.mkdir(result_data_path + test)
        with open(result_data_path + test + r'\qlen_data.csv', 'w', encoding='utf8') as w_file:    
            w_file.write('time'+','+'value'+'\n')
            with open(project_path + test + r'\qlen.txt', 'r', encoding='utf8') as r_file:
                while True:
                    line = r_file.readline()
                    if not line:
                        break;
                    data = line.split(' ')
                    now = 0.0;
                    if begin_time == 0.0:
                        begin_time = float(data[0][0:-1])
                        now = 0.0
                    else:
                        now = float(data[0][0:-1]) - begin_time
                    if len(data) > 1:
                        w_file.write(str(now) + ',' + data[1])
            r_file.close()     
        w_file.close()    
    
def read_rtt_data():
    for test in basic_lab:
        begin_time = 0.0
        if os.path.isdir(result_data_path + test):
            pass
        else:
            os.mkdir(result_data_path + test)
        with open(result_data_path + test + r'\rtt_data.csv', 'w', encoding='utf8') as w_file:    
            w_file.write('time'+','+'value'+'\n')
            with open(project_path + test + r'\rtt.txt', 'r', encoding='utf8') as r_file:
                while True:
                    line = r_file.readline()
                    if not line:
                        break;
                    data = line.split(' ')
                    now = 0.0;
                    ttl = 0.0
                    if begin_time == 0.0:
                        begin_time = float(data[0][0:-1])
                        now = 0.0
                    else:
                        now = float(data[0][0:-1]) - begin_time
                    for i in data:
                        if i.split('=')[0] == '时间':
                            ttl = float(i.split('=')[1])
                    w_file.write(str(now) + ',' + str(ttl) + '\n')
            r_file.close()     
        w_file.close()        

def read_brandwidth():
    for test in basic_lab:
        begin_time = 0.5
        if os.path.isdir(result_data_path + test):
            pass
        else:
            os.mkdir(result_data_path + test)
        with open(result_data_path + test + r'\iperf_result.csv', 'w', encoding='utf8') as w_file:    
            w_file.write('time'+','+'value'+'\n')
            with open(project_path + test + r'\iperf.txt', 'r', encoding='utf8') as r_file:
                r_file.readline()
                r_file.readline()
                r_file.readline()
                r_file.readline()
                r_file.readline()
                r_file.readline()
                r_file.readline()
                while True:
                    line = r_file.readline()
                    # print(line)
                    if not line:
                        break;
                    data = line.split(' ')
                    # print(data)
                    for i in range(len(data)):
                        # print(i,':',data[i])
                        pattern=r'Mbits/sec' 
                        patterns=r'bits/sec' 
                        if (data[i] == pattern or data[i] == patterns) :
                        # if re.match(pattern, data[i]):
                            index=i
                            # print(data[index-1])
                            break
                    w_file.write(str(begin_time) + ',' + str(float(data[index-1])) +'\n')
                    begin_time+=0.5
            r_file.close()     
        w_file.close()        

def read_pro():
    if os.path.isdir(pwd + pro_reault):
        pass
    else:
        os.mkdir(pwd + pro_reault)    
    for test in pro_lab:
        begin_time = 0.0
        print(pwd + pro_reault + '\\' + test[1:])
        with open(pwd + pro_reault + '\\' + test[1:] + '.csv', 'w', encoding='utf8') as w_file:
            w_file.write('time'+','+'value'+'\n')
            with open(pwd + test + r'\rtt.txt', 'r', encoding='utf8') as r_file:
                while True:
                    line = r_file.readline()
                    if not line:
                        break;
                    data = line.split(' ')
                    now = 0.0;
                    ttl = 0.0
                    if begin_time == 0.0:
                        begin_time = float(data[0][0:-1])
                        now = 0.0
                    else:
                        now = float(data[0][0:-1]) - begin_time
                    for i in data:
                        if i.split('=')[0] == '时间':
                            ttl = float(i.split('=')[1])
                            w_file.write(str(now) + ',' + str(ttl) + '\n')
            r_file.close()     
        w_file.close()                

# 读取数据
read_cwnd_data()
read_qlen_data()
read_rtt_data()
read_brandwidth()
read_pro()
# 画图
# write_table()


