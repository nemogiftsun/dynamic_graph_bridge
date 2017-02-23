#!/usr/bin/env python
import rospy
from dynamic_graph_bridge_msgs.msg import Vector
from cellularskin_msgs.msg import SkinPatch
from dynamic_graph.sot.core.utils.thread_interruptible_loop import loopInThread,loopShortcuts
import time

class proximity_range_bridge:
    def __init__(self):
        rospy.init_node('proximity_range_bridge', anonymous=True)
        self.proximity_vector = Vector()
        self.proximity_vector.data = [0.05,] *4
        self.pub = rospy.Publisher('proximity_data', Vector, queue_size=10)
        self.rate = rospy.Rate(10) # 10hz
       
    def run(self):
        self.proximity_vector.data 
        self.pub.publish(self.proximity_vector)
 	self.rate.sleep()


# Controller Thread

prb = proximity_range_bridge()

@loopInThread
def pub():
    prb.run()

runner=pub()
runner.once()
[go,stop,next,n]=loopShortcuts(runner)

def slowupanddown():
    error = 0.14
    for i in range(15): 
    	prb.proximity_vector.data = [error,] *7
        error = error + 0.01
        time.sleep(1)
        print prb.proximity_vector.data
    for j in range(20): 
    	prb.proximity_vector.data = [error,] * 7
        error = error - 0.01
        print prb.proximity_vector.data
        time.sleep(0.3)
        




