#!/usr/bin/env python3
"""复制生产源码到独立诊断目录并插入计时，不修改原项目或 SDK。"""
import argparse
from pathlib import Path
import shutil


def replace(text, before, after):
    if text.count(before) != 1:
        raise RuntimeError('Profiling anchor changed: '+before[:80])
    return text.replace(before, after, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    project, destination = args.project.resolve(), args.destination.resolve()
    destination.mkdir(parents=True) # 已存在时拒绝覆盖其它诊断/构建目录。
    for name in ('src', 'cmake'):
        shutil.copytree(project/name, destination/name)
    shutil.copyfile(project/'CMakeLists.txt', destination/'CMakeLists.txt')
    shutil.copyfile(Path(__file__).with_name('profile.h'), destination/'src/profile.h')

    path = destination/'src/app.cpp'
    code = '#include "profile.h"\n'+path.read_text()
    code = replace(code, '        auto frame = camera.acquire(150);',
        '        profile::begin(frames);\n'
        '        auto frame = [&] { profile::Scope timer(profile::Acquire); return camera.acquire(150); }();\n'
        '        const auto updateStart = profile::now();')
    code = replace(code, '        switch (frame.state()) {',
        '        profile::elapsed(profile::Update,updateStart);\n        switch (frame.state()) {')
    code = replace(code, '        case FrameStatus::Ready:', '        case FrameStatus::Ready: {')
    code = replace(code, '        case FrameStatus::Timeout:', '        }\n        case FrameStatus::Timeout:')
    code = replace(code, '            hud.clear();\n            menus.top().render(hud);',
        '            { profile::Scope timer(profile::Clear); hud.clear(); }\n'
        '            { profile::Scope timer(profile::Render); menus.top().render(hud); }\n'
        '            const auto composeStart = profile::now();')
    code = replace(code, '            if (!display.present(video.output(),error)) {',
        '            profile::elapsed(profile::Compose,composeStart);\n'
        '            const auto presentStart = profile::now();\n'
        '            if (!display.present(video.output(),error)) {')
    code = replace(code, '            ++frames;',
        '            profile::elapsed(profile::Present,presentStart);\n            ++frames;')
    code = replace(code, '        if (!menus.apply(std::move(action))) std::cerr << "Menu navigation rejected\\n";',
        '        { profile::Scope timer(profile::Navigate);\n'
        '          if (!menus.apply(std::move(action))) std::cerr << "Menu navigation rejected\\n"; }\n'
        '        profile::end();')
    code = replace(code, '    return 0;', '    profile::dump();\n    return 0;')
    path.write_text(code)

    path = destination/'src/media/spi_linux.cpp'
    code = '#include "profile.h"\n'+path.read_text()
    code = replace(code, '    return ::ioctl(fd,GPIOHANDLE_SET_LINE_VALUES_IOCTL,&values) == 0',
        '#if AA_PROFILE_DETAIL\n    const auto gpioStart = profile::now();\n#endif\n'
        '    const int result = ::ioctl(fd,GPIOHANDLE_SET_LINE_VALUES_IOCTL,&values);\n'
        '#if AA_PROFILE_DETAIL\n    const int saved = errno;\n'
        '    profile::elapsed(profile::Gpio,gpioStart);\n    errno = saved;\n#endif\n'
        '    return result == 0')
    code = replace(code, '    const auto written = ::write(spi,data,size);',
        '#if AA_PROFILE_DETAIL\n    const auto spiStart = profile::now();\n#endif\n'
        '    const auto written = ::write(spi,data,size);\n'
        '#if AA_PROFILE_DETAIL\n    const int saved = errno;\n'
        '    profile::elapsed(size > 4 ? profile::SpiPixels : profile::SpiSmall,spiStart);\n'
        '    errno = saved;\n#endif')
    path.write_text(code)

    path = destination/'src/media/display_gc9a01.cpp'
    code = '#include "profile.h"\n'+path.read_text()
    code = replace(code, '        for (int i = top*width; i < end*width; ++i) {',
        '#if AA_PROFILE_DETAIL\n        const auto packStart = profile::now();\n#endif\n'
        '        for (int i = top*width; i < end*width; ++i) {')
    code = replace(code, '        if (!bus.command(0x2A,columns,sizeof(columns),error)',
        '#if AA_PROFILE_DETAIL\n        profile::elapsed(profile::Pack,packStart);\n#endif\n'
        '        if (!bus.command(0x2A,columns,sizeof(columns),error)')
    path.write_text(code)
    print(destination)


if __name__ == '__main__':
    main()
