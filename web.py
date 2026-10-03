"""Set up, build and preview the browser interface on macOS, Linux or Windows."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
SDK = ROOT / 'build/emsdk'
SDK_VERSION = '6.0.10'


def setup():
    if not (SDK / 'emsdk.py').is_file():
        git = shutil.which('git')
        if not git:
            raise RuntimeError('Install Git before setting up Emscripten.')
        SDK.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([git, 'clone', '--depth', '1',
                        'https://github.com/emscripten-core/emsdk.git', str(SDK)], check=True)
    for action in ('install', 'activate'):
        subprocess.run([sys.executable, str(SDK / 'emsdk.py'), action, SDK_VERSION],
                       cwd=SDK, check=True)
    print('Web toolchain ready. Run web.py to build and preview ChessBot.', flush=True)


def build(configuration, compiler=None):
    if compiler:
        path = Path(compiler).expanduser()
        resolved = str(path.resolve()) if path.is_file() else shutil.which(compiler)
        if not resolved:
            raise RuntimeError(f'Compiler not found: {compiler}')
        compiler_path = Path(resolved)
    else:
        compiler_path = SDK / 'upstream/emscripten/emcc.py'
        if not compiler_path.is_file():
            raise RuntimeError('Emscripten is missing. Run web.py setup first, or pass --compiler.')

    environment = os.environ.copy()
    environment['EMSDK_PYTHON'] = sys.executable
    if compiler_path.parent == SDK / 'upstream/emscripten':
        config = SDK / '.emscripten'
        if not config.is_file():
            raise RuntimeError('Emscripten is not activated. Run web.py setup first.')
        environment['EM_CONFIG'] = str(config)

    output = ROOT / 'build/web'
    output.mkdir(parents=True, exist_ok=True)
    flags = ['-std=gnu11', '-Wall', '-Wextra', '-DCHESSBOT_WEB', '-I', str(ROOT)]
    flags += ['-O3'] if configuration == 'Release' else ['-O0', '-g', '-sASSERTIONS=2']
    flags += ['--no-entry', '-sMODULARIZE=1', '-sEXPORT_NAME=createChessBot',
              '-sENVIRONMENT=web,worker,node', '-sALLOW_MEMORY_GROWTH=1',
              '-sINITIAL_MEMORY=67108864', '-sMAXIMUM_MEMORY=268435456',
              '-sSTACK_SIZE=1048576', '-sFILESYSTEM=0',
              '-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString']
    sources = ['web/bridge.c', 'ui/Game.c', 'Board.c', 'Position.c',
               'Eval.c', 'Data.c', 'Zobrist.c']
    command = ([sys.executable, str(compiler_path)] if compiler_path.suffix == '.py'
               else [str(compiler_path)])
    print(f'Building ChessBot WebAssembly ({configuration})', flush=True)
    subprocess.run(command + flags + [str(ROOT / name) for name in sources] +
                   ['-o', str(output / 'engine.js'), '-lm'],
                   cwd=ROOT, env=environment, check=True)
    for path in (ROOT / 'web').iterdir():
        if path.is_file() and path.suffix in ('.html', '.css', '.js'):
            shutil.copy2(path, output / path.name)
    pieces = output / 'pieces'
    pieces.mkdir(exist_ok=True)
    for path in (ROOT / 'web/pieces').iterdir():
        if path.is_file():
            shutil.copy2(path, pieces / path.name)
    print(f'Static website created in {output}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', nargs='?', choices=('setup', 'build', 'run'), default='run')
    parser.add_argument('--configuration', choices=('Release', 'Debug'), default='Release')
    parser.add_argument('--compiler', help='Use an existing emcc or emcc.py instead of the local SDK')
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--skip-build', action='store_true', help='Serve the existing build')
    args = parser.parse_args()
    if sys.version_info < (3, 10):
        parser.error('Python 3.10 or newer is required.')
    if not 1024 <= args.port <= 65535:
        parser.error('--port must be between 1024 and 65535')
    try:
        if args.action == 'setup':
            setup()
        else:
            if args.action == 'build' or not args.skip_build:
                build(args.configuration, args.compiler)
            if args.action == 'run':
                subprocess.run([sys.executable, str(ROOT / 'helpers/serve_web.py'),
                                '--port', str(args.port)], check=True)
    except KeyboardInterrupt:
        return 0
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Web launcher failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
