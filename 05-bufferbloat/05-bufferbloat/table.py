# -*- coding: utf-8 -*-
import csv
import os
import numpy as np
from turtle import color
import matplotlib.pyplot as mp
from matplotlib.font_manager import FontProperties #字体管理器
 
#设置汉字格式
font = FontProperties(fname=r"c:\windows\fonts\simsun.ttc", size=15)
project_path = os.path.dirname(os.path.abspath(__file__)) + \
               r'\qlen-'
               
result_data_path = os.path.dirname(os.path.abspath(__file__)) + \
               r'\data-len-'
final_data_path = os.path.dirname(os.path.abspath(__file__)) + \
               r'\final_data'
pro_data_path = os.path.dirname(os.path.abspath(__file__)) + \
               r'\pro'

picture_path = os.path.dirname(os.path.abspath(__file__)) + \
               r'\table'
pro_lab = ['taildrop', 'red', 'codel']
# len = 20
qlen_cwnd_time = dict()
qlen_cwnd_value = dict()
qlen_qlen_time = dict()
qlen_qlen_value = dict()
qlen_rtt_time = dict()
qlen_rtt_value = dict()
brand_time = dict()
brand_value = dict()

pro_time = dict()
pro_value = dict()

def get_all_data():
    dir = ['20', '40', '60', '80', '100']
    for each in dir:
        data_file = result_data_path + each
        with open(data_file + r'\cwnd_data.csv', 'r', encoding='utf8') as file:
            reader_1 = csv.DictReader(file)
            qlen_cwnd_time[each] = [float(row['time']) for row in reader_1]
        file.close()
        with open(data_file + r'\cwnd_data.csv', 'r', encoding='utf8') as file:
            reader_2 = csv.DictReader(file)
            qlen_cwnd_value[each] = [float(row['value']) for row in reader_2]
        file.close()

        with open(data_file + r'\qlen_data.csv', 'r', encoding='utf8') as file:
            reader_1 = csv.DictReader(file)
            qlen_qlen_time[each] = [float(row['time']) for row in reader_1]
        file.close()
        with open(data_file + r'\qlen_data.csv', 'r', encoding='utf8') as file:
            reader_2 = csv.DictReader(file)
            qlen_qlen_value[each] = [float(row['value']) for row in reader_2]
        file.close()

        with open(data_file + r'\rtt_data.csv', 'r', encoding='utf8') as file:
            reader_1 = csv.DictReader(file)
            qlen_rtt_time[each] = [float(row['time']) for row in reader_1]
        file.close()
        with open(data_file + r'\rtt_data.csv', 'r', encoding='utf8') as file:
            reader_2 = csv.DictReader(file)
            qlen_rtt_value[each] = [float(row['value']) for row in reader_2]
        file.close()
        with open(data_file + r'\iperf_result.csv', 'r', encoding='utf8') as file:
            reader_2 = csv.DictReader(file)
            brand_time[each] = [float(row['time']) for row in reader_2]
        file.close()
        with open(data_file + r'\iperf_result.csv', 'r', encoding='utf8') as file:
            reader_2 = csv.DictReader(file)
            brand_value[each] = [float(row['value']) for row in reader_2]
        file.close()
    # 得到pro的值
    for each in pro_lab:
        print(pro_data_path + '\\' + each + '.csv')
        with open(pro_data_path + '\\' + each + '.csv', 'r', encoding='utf8') as file:
            reader_1 = csv.DictReader(file)
            pro_time[each] = [float(row['time']) for row in reader_1]
        # print(pro_time[each])
        file.close()
        with open(pro_data_path + '\\' + each + '.csv', 'r', encoding='utf8') as file:
            reader_2 = csv.DictReader(file)
            pro_value[each] = [float(row['value']) for row in reader_2]
        # print(pro_value[each])
        file.close()        
      


def draw_table_basic():
    # 前三个演示图
    mp.title("CWND's time plot")    
    mp.xlabel('time(s)')
    mp.ylabel('CWND(KB)')
    mp.plot(qlen_cwnd_time['100'], qlen_cwnd_value['100'], label=r'$maxq=100$',color = 'slateblue')
    mp.legend(loc = 0)
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)

    mp.savefig(picture_path + r'\CWND_100MB.jpg')
    mp.show()    
    
    mp.title("Qlen's time plot")    
    mp.xlabel('time(s)')
    mp.ylabel('Qlen: #(packets)')
    mp.plot(qlen_qlen_time['100'], qlen_qlen_value['100'], label=r'$maxq=100$',color = 'orange')
    mp.legend(loc = 0)
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    mp.ylim(0, 150)
    mp.savefig(picture_path + r'\Qlen_100MB.jpg')
    mp.show()   
     
    mp.title("RTT's time plot")
    mp.xlabel('time(s)')
    mp.ylabel('RTT(ms)')
    mp.plot(qlen_rtt_time['100'], qlen_rtt_value['100'], label=r'$maxq=100$',color = 'mediumorchid')
    mp.legend(loc = 0)
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    # mp.ylim(0, 600)
    mp.savefig(picture_path + r'\RTT_100MB.jpg')
    mp.show()   
    
    # 后四个对比图
    mp.title("CWND's graph with queue size")
    mp.xlabel('time(S)')
    mp.ylabel('CWND(KB)')
    mp.plot(qlen_cwnd_time['20'], qlen_cwnd_value['20'], label=r'$maxq=20$')#,color = 'orange')
    mp.plot(qlen_cwnd_time['40'], qlen_cwnd_value['40'], label=r'$maxq=40$')#,color = 'orange')
    mp.plot(qlen_cwnd_time['60'], qlen_cwnd_value['60'], label=r'$maxq=60$')#,color = 'orange')
    mp.plot(qlen_cwnd_time['80'], qlen_cwnd_value['80'], label=r'$maxq=80$')#,color = 'orange')
    mp.plot(qlen_cwnd_time['100'], qlen_cwnd_value['100'], label=r'$maxq=100$')#,color = 'orange')
    mp.legend(loc = 0)
    # mp.ylim(0, 150)
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    mp.savefig(picture_path + r'\CWND.jpg')
    mp.show()    
    
    mp.title("RTT's graph with queue size")
    mp.xlabel('time(S)')
    mp.ylabel('RTT(ms)')
    mp.plot(qlen_rtt_time['20'], qlen_rtt_value['20'], label=r'$maxq=20$')#,color = 'orange')
    mp.plot(qlen_rtt_time['40'], qlen_rtt_value['40'], label=r'$maxq=40$')#,color = 'orange')
    mp.plot(qlen_rtt_time['60'], qlen_rtt_value['60'], label=r'$maxq=60$')#,color = 'orange')
    mp.plot(qlen_rtt_time['80'], qlen_rtt_value['80'], label=r'$maxq=80$')#,color = 'orange')
    mp.plot(qlen_rtt_time['100'], qlen_rtt_value['100'], label=r'$maxq=100$')#,color = 'orange')
    mp.legend(loc = 0)
    # mp.ylim(0, 150)
    mp.ylim(0, 430)
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    mp.savefig(picture_path + r'\RTT.jpg')
    mp.show() 
        
    mp.title("Qlen's graph with queue size")
    mp.xlabel('time(S)')
    mp.ylabel('Qlen: #(packets)')
    mp.plot(qlen_qlen_time['20'], qlen_qlen_value['20'], label=r'$maxq=20$')#,color = 'orange')
    mp.plot(qlen_qlen_time['40'], qlen_qlen_value['40'], label=r'$maxq=40$')#,color = 'orange')
    mp.plot(qlen_qlen_time['60'], qlen_qlen_value['60'], label=r'$maxq=60$')#,color = 'orange')
    mp.plot(qlen_qlen_time['80'], qlen_qlen_value['80'], label=r'$maxq=80$')#,color = 'orange')
    mp.plot(qlen_qlen_time['100'], qlen_qlen_value['100'], label=r'$maxq=100$')#,color = 'orange')
    mp.legend(loc = 0)
    mp.ylim(0, 170)
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    mp.savefig(picture_path + r'\Qlen.jpg')
    mp.show()   
    
    mp.title("throughput's graph with queue size") 
    mp.xlabel('time(S)')
    mp.ylabel('ThroughputRate(Mbits/sec)')  
    mp.plot(brand_time['20'], brand_value['20'], label=r'$maxq=20$')#,color = 'orange')
    mp.plot(brand_time['40'], brand_value['40'], label=r'$maxq=40$')#,color = 'orange')
    mp.plot(brand_time['60'], brand_value['60'], label=r'$maxq=60$')#,color = 'orange')
    mp.plot(brand_time['80'], brand_value['80'], label=r'$maxq=80$')#,color = 'orange')
    mp.plot(brand_time['100'],brand_value['100'], label=r'$maxq=100$')#,color = 'orange')     
    mp.legend(loc = 0)    
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    mp.savefig(picture_path + r'\ThroughtputRate.jpg')
    mp.show()       

    mp.xlabel('time(S)')
    mp.ylabel('RTT(ms)')  
    mp.plot(pro_time['taildrop'], pro_value['taildrop'], label=r'$taildrop$')#,color = 'orange')
    mp.plot(pro_time['red'], pro_value['red'], label=r'$red$')#,color = 'orange')
    mp.plot(pro_time['codel'], pro_value['codel'], label=r'$codel$')#,color = 'orange') 
    mp.legend(loc = 0)    
    mp.yticks()
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    mp.savefig(picture_path + r'\DiffQueuqManagement.jpg')
    mp.show()           

    mp.xlabel('time(S)')
    mp.ylabel('log(RTT) (ms)')  
    mp.plot(pro_time['taildrop'], np.log(pro_value['taildrop']), label=r'$taildrop$')#,color = 'orange')
    mp.plot(pro_time['red'], np.log(pro_value['red']), label=r'$red$')#,color = 'orange')
    mp.plot(pro_time['codel'], np.log(pro_value['codel']), label=r'$codel$')#,color = 'orange') 
    mp.legend(loc = 0) 
    # mp.yscale("log")  
    ax = mp.gca()
    ax.spines['right'].set_color(None)
    ax.spines['top'].set_color(None)
    mp.savefig(picture_path + r'\DiffQueuqManagementlog.jpg')
    mp.show()   


get_all_data()
draw_table_basic()