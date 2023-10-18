from tqdm import tqdm
import hetu as ht
import numpy as np
from hetu.nn.modules.parallel import parallel_data_provider
import ptvsd # used for debug

ds_split01 = ht.DistributedStates(4, {0: 2, 1: 2}, [0, 1])

ht.init_comm_group()
local_device = ht.local_device()
all_devices = ht.global_device_group()
all_device_group = ht.DeviceGroup([all_devices.get(0), all_devices.get(1), all_devices.get(2), all_devices.get(3)])
local_device_index = all_device_group.get_index(local_device)
devices_num = all_device_group.num_devices

def unit_test():

    x = ht.parallel_placeholder(ht.int64, global_shape=[4, 6, 10], ds=ds_split01, device_group=all_device_group)
    y = ht.parallel_placeholder(ht.int64, global_shape=[4, 6, 2], ds=ds_split01, device_group=all_device_group)
    z = ht.dynamic_concat([x, y], axis=-1)

    x_data = ht.numpy_to_NDArray(np.ones((2, 3, 10)) * 5, dynamic_shape=[2, 3, 4])
    y_data = ht.numpy_to_NDArray(np.ones((2, 3, 2)) * 2, dynamic_shape=[2, 3, 1])

    feed_dict = {x: x_data, y: y_data}
    results = z.graph.run(z, [z], feed_dict)
    z_data = results[0].numpy(force=True)
    print(z_data)
    
if __name__ == '__main__':
    with ht.graph("define_and_run"):
        unit_test()