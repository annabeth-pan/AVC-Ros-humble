# SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
# Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

import os
import yaml

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource


def generate_launch_description():
    # Load configuration from YAML file
    config_path = os.path.join(os.path.dirname(__file__), 'yoloconf.yaml')
    with open(config_path, 'r') as f:
        config_yolo = yaml.safe_load(f)
    
    config_path = os.path.join(os.path.dirname(__file__), 'essconf.yaml')
    with open(config_path, 'r') as f:
        config_ess = yaml.safe_load(f)
    
    # Include the yolaunch.py with arguments from config
    yolaunch_path = os.path.join(os.path.dirname(__file__), 'yolobringup.py')
    yolo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([yolaunch_path]),
        launch_arguments={k: str(v) for k, v in config_yolo.items()}.items(),
    )
    
    # Include the ess.py with arguments from config
    ess_path = os.path.join(os.path.dirname(__file__), 'essbringup.py')
    ess = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([ess_path]),
        launch_arguments={k: str(v) for k, v in config_ess.items()}.items(),
    )
    
    return LaunchDescription([yolo, ess])