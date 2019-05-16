#!/usr/bin/env python
import rospy, sys
from blam_slam.srv import Restart

def connect():
    rospy.init_node('restart_client')
    restart = rospy.ServiceProxy('/husky/blam_slam/restart', Restart)
    if restart('pointcloud_backup.zip').success:
        print('Successfully restarted from graph')
    else:
        print('Error: pointcloud_backup.zip missing')

if __name__ == '__main__':
    try:
        if len(sys.argv) < 1:
            print('Usage: %s filename.zip' % sys.argv[0])
        else:
            connect()
    except rospy.ROSInterruptException: pass
