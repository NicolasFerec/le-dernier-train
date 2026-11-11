"""Splash screen and updater for the installed game."""

import errno
import logging
import queue
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import ttk

from update import (
    AlreadyRunning, DEFAULT_REPO, Installation, fetch_release, installation_lock,
    launch_game, restart_launcher, select_launcher_asset, stage_launcher, state_directory,
)

ART = Path(__file__).resolve().parent
PANEL = '#151d27'
INK = '#f4e6d3'
COPPER = '#d3a171'
TEXT = {
    'check': ('Vérification des mises à jour…', 'Checking for updates…'),
    'download': ('Téléchargement de la mise à jour…', 'Downloading the update…'),
    'install': ('Installation de la mise à jour…', 'Installing the update…'),
    'download_launcher': ('Téléchargement du lanceur…', 'Downloading the launcher…'),
    'install_launcher': ('Installation du lanceur…', 'Installing the launcher…'),
    'launch': ('Lancement du jeu…', 'Starting the game…'),
    'fallback': ('Mise à jour indisponible. Lancement de la version installée…',
                 'Update unavailable. Starting the installed version…'),
    'no_game': ('Aucune version disponible. Vérifie ta connexion.',
                'No version available. Check your connection.'),
    'no_space': ('Il manque de la place pour installer le jeu.', 'There is not enough disk space to install the game.'),
    'launch_error': ('Le jeu ne démarre pas. Consulte launcher.log dans le dossier des données.',
                     'The game could not start. See launcher.log in the data directory.'),
    'play': ('Lancer', 'Play'),
    'retry': ('Réessayer', 'Try again'),
}


def preference(path, choices, default):
    try:
        value = path.read_text(encoding='utf-8').strip()
        if value in choices:
            return value
    except (OSError, UnicodeError):
        pass
    return default


class Splash:
    def __init__(self, root, installation, settings, repo, executable, launcher_version):
        self.root = root
        self.installation = installation
        self.settings = settings
        self.repo = repo
        self.executable = executable
        self.launcher_version = launcher_version
        self.bundled_game = installation.bootstrap_asset()
        self.language = preference(settings / 'language.txt', ('fr', 'en'), 'fr')
        self.events = queue.SimpleQueue()
        self.process = None
        self.busy = False
        root.title('Le Dernier Train')
        root.configure(bg=PANEL)
        root.resizable(False, False)
        root.geometry(f'640x360+{max(0, (root.winfo_screenwidth() - 640) // 2)}'
                      f'+{max(0, (root.winfo_screenheight() - 360) // 2)}')
        self.background = tk.PhotoImage(file=ART / 'depot.png')
        self.icon = tk.PhotoImage(file=ART / 'icon.png')
        root.iconphoto(True, self.icon)
        canvas = tk.Canvas(root, width=640, height=360, highlightthickness=0, bg=PANEL)
        canvas.pack(fill='both', expand=True)
        canvas.create_image(0, 0, image=self.background, anchor='nw')
        canvas.create_rectangle(0, 237, 640, 360, fill=PANEL, outline='')
        canvas.create_line(0, 237, 640, 237, fill=COPPER)
        canvas.create_rectangle(25, 24, 325, 79, fill=PANEL, outline=COPPER)
        canvas.create_text(45, 52, text='Le Dernier Train', anchor='w', fill=INK, font=('serif', 23, 'bold'))
        self.status = canvas.create_text(25, 267, anchor='w', fill=INK, font=('sans-serif', 11), width=590)
        style = ttk.Style(root)
        style.theme_use('clam')
        style.configure('Train.Horizontal.TProgressbar', troughcolor='#343c46', background=COPPER,
                        bordercolor=PANEL, lightcolor=COPPER, darkcolor=COPPER, thickness=5)
        style.configure('Train.TButton', background=PANEL, foreground=INK, bordercolor=COPPER)
        style.map('Train.TButton', background=[('active', '#303b46')])
        self.progress = ttk.Progressbar(root, style='Train.Horizontal.TProgressbar', length=590, maximum=100)
        self.progress.place(x=25, y=292)
        self.button = ttk.Button(root, style='Train.TButton', text=self.text('play'), command=self.start)
        self.button.place(x=502, y=316, width=112)
        self.canvas = canvas
        root.after(450, self.start)
        root.after(50, self.poll)

    def text(self, key):
        return TEXT[key][self.language == 'en']

    def start(self):
        if self.busy:
            return
        self.busy = True
        self.button.configure(state='disabled')
        threading.Thread(target=self.check_updates, daemon=True).start()

    def check_updates(self, release=None):
        def progress(key, value):
            self.events.put(('progress', key, value))

        no_space = False
        try:
            self.installation.bootstrap(self.bundled_game, progress)
            self.bundled_game = None
        except (OSError, ValueError) as error:
            no_space = getattr(error, 'errno', None) in (errno.ENOSPC, getattr(errno, 'EDQUOT', -1))
            logging.exception('Unable to install the bundled game')
        try:
            if release is None:
                progress('check', None)
                release = fetch_release(self.repo)
                if self.executable:
                    try:
                        asset = select_launcher_asset(release, self.repo, self.launcher_version)
                        if asset:
                            candidate = stage_launcher(asset, self.executable, self.repo, progress)
                            self.events.put(('restart', candidate, release))
                            return
                    except Exception:
                        logging.exception('Unable to update the launcher')
            game = self.installation.update(self.repo, progress, release=release)
            self.events.put(('ready', game, False))
        except Exception as error:
            logging.exception('Unable to update the game')
            installed = self.installation.current()
            if installed:
                self.events.put(('ready', installed[2], True))
            else:
                no_space |= getattr(error, 'errno', None) in (errno.ENOSPC, getattr(errno, 'EDQUOT', -1))
                self.events.put(('error', 'no_space' if no_space else 'no_game', None))

    def poll(self):
        while not self.events.empty():
            event, data, value = self.events.get_nowait()
            if event == 'progress':
                self.canvas.itemconfigure(self.status, text=self.text(data))
                self.progress.configure(value=0 if value is None else value * 100)
            elif event == 'ready':
                self.progress.configure(value=100)
                self.canvas.itemconfigure(self.status, text=self.text('fallback' if value else 'launch'))
                self.root.after(2200 if value else 200, self.play, data)
            elif event == 'error':
                self.show_error(data)
            elif event == 'restart':
                try:
                    restart_launcher(self.executable, data)
                except OSError:
                    logging.exception('Unable to restart the launcher')
                    data.unlink(missing_ok=True)
                    threading.Thread(target=self.check_updates, args=(value,), daemon=True).start()
        self.root.after(50, self.poll)

    def show_error(self, message):
        self.busy = False
        self.canvas.itemconfigure(self.status, text=self.text(message))
        self.progress.configure(value=0)
        self.button.configure(text=self.text('retry'), state='normal')

    def play(self, game):
        try:
            self.process = launch_game(self.installation, game, self.settings)
        except OSError:
            logging.exception('Unable to launch the game')
            self.show_error('launch_error')
            return
        self.root.withdraw()
        self.root.after(5500, self.finish_startup)

    def finish_startup(self):
        exit_code = self.process.poll()
        if exit_code is None or exit_code == 0:
            self.root.destroy()
            return
        logging.error('The game exited during startup with code %s', exit_code)
        self.process = None
        self.root.deiconify()
        self.show_error('launch_error')


def main():
    repo_path = ART / 'repository.txt'
    repo = repo_path.read_text(encoding='utf-8').strip() if repo_path.is_file() else DEFAULT_REPO
    version = (ART / 'version.txt').read_text(encoding='utf-8').strip()
    platform = 'windows' if sys.platform == 'win32' else 'linux'
    frozen = getattr(sys, 'frozen', False)
    executable = Path(sys.executable).resolve() if frozen and platform == 'linux' else None
    if sys.argv[1:2] == ['--check-launcher']:
        if not executable or sys.argv[2:] != [version, repo]:
            return 1
        window = tk.Tk()
        window.withdraw()
        tk.PhotoImage(file=ART / 'depot.png')
        tk.PhotoImage(file=ART / 'icon.png')
        ttk.Style(window).theme_use('clam')
        window.destroy()
        return 0
    root = (Path(sys.executable).resolve().parent if frozen
            else ART.parents[1] / 'Saved/Distribution/LauncherDevelopment')
    try:
        with installation_lock(root):
            settings = state_directory()
            settings.mkdir(parents=True, exist_ok=True)
            logging.basicConfig(filename=settings / 'launcher.log', filemode='w', encoding='utf-8', level=logging.INFO)
            window = tk.Tk()
            splash = Splash(window, Installation(root, platform), settings, repo, executable, version)
            if executable:
                executable.with_name('.launcher-previous').unlink(missing_ok=True)
                executable.with_name('.launcher-next').unlink(missing_ok=True)
            window.mainloop()
            if splash.process:
                return splash.process.wait()
    except AlreadyRunning:
        return 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
