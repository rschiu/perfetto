// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as m from 'mithril';

import {timeToString} from '../common/time';

import {globals} from './globals';
import {gridlines} from './gridline_helper';
import {Panel, PanelSize} from './panel';
import {TRACK_SHELL_WIDTH} from './track_constants';

export class TimeAxisPanel extends Panel {
  view() {
    return m(
        '.time-axis-panel',
        m('.time-offset',
          `${
             timeToString(
                 globals.frontendLocalState.visibleWindowTime.start -
                 globals.state.traceTime.startSec)
           }`));
  }


  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const timeScale = globals.frontendLocalState.timeScale;
    const range = globals.frontendLocalState.visibleWindowTime;
    ctx.font = '10px Google Sans';
    ctx.fillStyle = '#999';

    ctx.fillRect(TRACK_SHELL_WIDTH - 1, 0, 2, size.height);
    for (const [x, time] of gridlines(size.width, range, timeScale)) {
      ctx.fillRect(x, 0, 1, size.height);
      ctx.fillText('+' + timeToString(time - range.start), x + 5, 10);
    }
  }
}
