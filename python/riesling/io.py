import h5py
import xarray as xr
import numpy as np

INFO_FIELDS = ['matrix', 'voxel_size', 'origin', 'direction', 'tr']
INFO_FORMAT = [('<i8', (3,)), ('<f4', (3,)), ('<f4', (3,)), ('<f4', (3, 3)), '<f4']
INFO_DTYPE = np.dtype({'names': INFO_FIELDS, 'formats': INFO_FORMAT})

def write(filename, data=None, trajectory=None, info=None, meta=None, compression='gzip'):
    with h5py.File(filename, 'w') as out_f:
        out_f.create_dataset('data', dtype='c8', data=data.data, chunks=np.shape(data.data), compression=compression)
        if trajectory is not None:
            if trajectory.ndim != 3:
                AssertionError('Trajectory must be 3 dimensional (co-ords, samples, traces)')
            if trajectory.shape[2] > 3:
                AssertionError('Trajectory cannot have more than 3 co-ordinates')
            out_f.create_dataset('trajectory', dtype='f4', data=data.data, compression=compression)
        if info is not None:
            out_f.create_dataset('info', data=np.array([[info[f] for f in INFO_FIELDS]], dtype=INFO_DTYPE))
        if meta is not None:
            meta_g = out_f.create_group('meta')
            for k, v in meta:
                meta_g.create_dataset(k, data=v)

def read(filename, dset=None):
    with h5py.File(filename) as f:
        ret = []
        data = None
        dims = []
        if dset is None:
            dset = 'data'
        data = xr.DataArray(f[dset], dims=[d.label for d in f[dset].dims])
        ret.append(data)

        if 'info' in f.keys():
            info_dset = np.array(f['info'], dtype=INFO_DTYPE)[0]
            info_dict = {}
            for key, item in zip(INFO_FIELDS, info_dset):
                info_dict[key] = item
            ret.append(info_dict)
        
        if 'trajectory' in f.keys():
            traj = np.array(f['trajectory'])
            ret.append(traj)
        
        if 'meta' in f.keys():
            meta = {}
            for k in f['meta'].keys():
                meta[key] = f['meta'][key][0]
            ret.append(meta)

        return ret[:]
