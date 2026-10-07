#!/usr/bin/env python3
"""Bounded browser checks for the standalone website demo, never handheld proof.

pip install playwright; playwright install chromium
python tools/test-web-demo.py [--browser PATH] [--screenshots PATH]
"""
import argparse
import functools
import http.server
from pathlib import Path
import threading
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *_):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser')
    parser.add_argument('--screenshots', type=Path)
    args = parser.parse_args()
    handler = functools.partial(QuietHandler, directory=str(ROOT / 'web-demo'))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    errors, failures = [], []
    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(executable_path=args.browser, headless=True)
            context = browser.new_context(viewport={'width': 1440, 'height': 1120}, reduced_motion='reduce')
            page = context.new_page()
            page.on('pageerror', lambda e: errors.append(str(e)))
            page.on('response', lambda r: failures.append(f'{r.status} {r.url}') if r.status >= 400 else None)
            page.goto(f'http://127.0.0.1:{server.server_port}/')
            page.evaluate('document.fonts.ready')
            page.clock.install()

            def click(action, value=None):
                selector = f'[data-action="{action}"]'
                if value is not None:
                    selector += f'[data-value="{value}"]'
                page.locator(selector).first.click()

            def state():
                return page.evaluate("JSON.parse(localStorage.getItem('traineros.website.demo.v1'))")

            def shot(name):
                assert page.locator('#screen').evaluate('e => e.scrollTop') == 0, name
                if args.screenshots:
                    args.screenshots.mkdir(parents=True, exist_ok=True)
                    page.locator('#device').screenshot(path=str(args.screenshots / f'{name}.png'))

            def face(name):
                click('face', name)

            def primary(name):
                click('page', name)

            # Initial page, complete shell fit and actual image/font requests.
            assert page.locator('.big-play').is_visible()
            assert page.locator('.chassis-footer').bounding_box()['y'] < 1100
            shot('01-home')
            page.locator('#choose').click()
            click('select-home', 'tide')
            assert 'Tide Almanac' in page.locator('.home-subtitle').inner_text()
            primary('worlds')
            shot('02-worlds')
            click('collection', 'vale')
            click('wheel', '-1')
            assert 'Garden Stories' in page.locator('.game-detail h3').inner_text()
            click('wheel', '1')
            assert 'Bloom Trails' in page.locator('.game-detail h3').inner_text()
            shot('03-library')
            click('properties')
            click('rename', 'bloom')
            page.locator('#game-name').fill('A <bright> adventure')
            click('save-name', 'bloom')
            assert page.locator('.game-detail h3').inner_text() == 'A <bright> adventure'
            assert page.locator('.game-detail bright').count() == 0
            click('launch', 'bloom')
            assert page.locator('.game-scene').is_visible()
            click('home-menu')
            click('picture')
            click('toggle', 'scanlines')
            click('close')
            assert page.locator('.game-scene.scanlines').count() == 1
            click('home-menu')
            click('exit')
            click('exit-confirm')

            primary('companions')
            click('creature', '1')
            click('favorite', '1')
            assert 1 in state()['favorites']
            shot('04-guide')
            face('party')
            click('party-detail', '2')
            click('to-box', '2')
            assert 2 not in state()['party'] and 2 in state()['boxes']
            face('boxes')
            click('box-detail', '2')
            click('to-party', '2')
            assert 2 in state()['party'] and 2 not in state()['boxes']
            face('center')
            click('heal')
            page.clock.run_for(2400)
            assert state()['health'][0] == 78
            shot('05-center')
            face('playroom')
            shot('06-playroom')
            face('shops')
            before = state()['money']
            for _ in range(3):
                click('cart-add', '0:0')
            click('cart-add', '0:1')
            shot('07-shop')
            click('checkout')
            assert state()['money'] == before - 420
            assert state()['bag']['Trail tonic'] == 3
            assert state()['bag']['Berry biscuit'] == 1
            for f in ['profile', 'journey', 'hall', 'ra']:
                primary('trainer')
                face(f)
                assert page.locator('#screen').inner_text().strip()

            primary('social')
            assert page.locator('#composer').is_visible()
            page.locator('#composer input').fill('<hello & friends>')
            page.locator('#composer button[type="submit"]').click()
            page.clock.run_for(1300)
            assert '<hello & friends>' in page.locator('.messages').inner_text()
            assert page.locator('.messages hello').count() == 0
            shot('08-social')
            face('groups')
            click('call')
            assert page.locator('.call-pill').is_visible()
            face('communities')
            assert page.locator('#composer').is_visible()
            face('search')
            assert page.locator('.contacts').count() == 0
            page.locator('#search-form input').fill('Mira')
            page.locator('#search-form button').click()
            click('open-chat', 'Mira')
            click('invite')
            click('online')
            click('send-invite')
            page.clock.run_for(2200)
            assert page.locator('.game-scene .joined').is_visible()
            click('home-menu')
            assert 'group call continues' in page.locator('.modal').inner_text()
            shot('09-game-menu')
            click('exit')
            click('exit-confirm')
            click('system')
            click('settings')
            click('theme', 'red')
            assert state()['theme'] == 'red'
            shot('10-settings')
            for category in ['Sound','Trainer','Library','Saves','Connections','Communication','Credits']:
                click('setting-category', category)
                assert page.locator('.settings-content').inner_text().strip()
            click('close')
            page.reload()
            assert page.locator('#device').evaluate("e=>e.style.getPropertyValue('--top')") == '#984b51'
            assert '<hello & friends>' in state()['messages']['Mira'][0]['text']
            # All guided routes finish; navigation cancels future steps.
            routes=[('worlds','collections'),('companions','guide'),('companions','party'),('companions','center'),('companions','playroom'),('companions','shops'),('trainer','journey'),('social','messages')]
            for section, sub in routes:
                primary(section)
                face(sub)
                page.locator('#example').click()
                page.clock.run_for(14000)
                assert 'Пример завершён' in page.locator('#scene-status').inner_text(), (section, sub)
                if page.locator('.close').count():
                    click('close')
            primary('social')
            page.locator('#example').click()
            page.clock.run_for(50)
            page.locator('#example').click()
            primary('worlds')
            page.clock.run_for(14000)
            assert 'Your worlds' in page.locator('#screen').inner_text()
            primary('companions')
            face('center')
            click('link')
            click('trade')
            click('trade-example')
            page.clock.run_for(2300)
            assert page.locator('.exchanging').count() == 1
            page.clock.run_for(6000)
            assert page.locator('.modal').count() == 0
            # Desktop, narrow landscape and phone: preserve one horizontal shell.
            for width,height in [(1280,800),(900,600),(390,844)]:
                page.set_viewport_size({'width':width,'height':height})
                primary('home')
                page.clock.run_for(100)
                page.wait_for_function('document.querySelector("#device").getBoundingClientRect().right <= innerWidth + 1')
                bounds=page.locator('#device').bounding_box()
                assert bounds['x'] >= 0 and bounds['x']+bounds['width'] <= width+1, (width,height,bounds)
                assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
            page.set_viewport_size({'width':1440,'height':1120})
            page.locator('#reset').click()
            click('reset-confirm')
            assert state()['theme'] == 'turquoise' and state()['money'] == 2400
            assert not state()['messages']['Mira']
            if args.screenshots:
                page.screenshot(path=str(args.screenshots/'00-full-demo.png'),full_page=True)
            assert not errors, errors
            assert not failures, failures
            browser.close()
            print('PASS: navigation, wrap, launch/exit, guide, storage, healing, basket, chat, invitations, persistent settings, scripted tours/cancellation, escaping, reset and 4 viewports; no page errors or failed asset requests.')
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
