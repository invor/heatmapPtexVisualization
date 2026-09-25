import numpy as np
import pandas as pd

df = pd.read_csv("E:/github/heatmapPtexVisualization/data_utils/2022_02_11-14_04_46-Nelusa-Trial.csv")

column_filter_df = df[['frameTimestamp', 'gazePoint_x', 'gazePoint_y', 'gazePoint_z']]
nan_filter_df = column_filter_df[column_filter_df['gazePoint_x'].notnull()]

print(nan_filter_df)

#nan_filter_df.to_hdf("C:/Users/micha/Documents/GitHub/heatmapPtexVisualization/data_utils/2022_08_05-15_01_26-Michael-gaze_points.hdf5", key="data", mode="w", format = 'table', data_columns=True, index=False)
nan_filter_df.to_csv("E:/github/heatmapPtexVisualization/data_utils/2022_02_11-14_04_46-Nelusa-Trial-gaze_points.csv")