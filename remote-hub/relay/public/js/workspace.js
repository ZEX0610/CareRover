import { t, onLangChange } from './i18n.js';

/** Layout and appearance; device modes still go through app.js and confirmed telemetry. */
export function initWorkspace({ beforeViewChange, signal }) {
  const app = document.getElementById('app');
  const sidebar = document.getElementById('sidebar');
  const toggle = document.getElementById('sidebarToggle');
  const narrow = matchMedia('(max-width: 600px)');
  const on = (el, event, fn) => el.addEventListener(event, fn, { signal });
  const setCollapsed = collapsed => {
    document.body.dataset.sidebar = collapsed ? 'collapsed' : 'expanded';
    toggle.setAttribute('aria-expanded', String(!collapsed));
    sidebar.inert = collapsed && narrow.matches;
  };
  setCollapsed(narrow.matches);
  on(narrow, 'change', () => setCollapsed(narrow.matches));
  on(toggle, 'click', () => setCollapsed(document.body.dataset.sidebar !== 'collapsed'));
  const refreshTitle = () => { document.getElementById('viewTitle').textContent = t(`nav.${app.dataset.view}`); };
  const offLang = onLangChange(refreshTitle);
  const selectView = view => {
    beforeViewChange();
    app.dataset.view = view;
    sidebar.querySelectorAll('[data-view]').forEach(button => {
      if (button.dataset.view === view) button.setAttribute('aria-current', 'page');
      else button.removeAttribute('aria-current');
    });
    if (view === 'debug') document.getElementById('debugPanel').open = true;
    refreshTitle();
    window.scrollTo({ top: 0, behavior: 'instant' });
    if (narrow.matches) { setCollapsed(true); toggle.focus(); }
  };
  sidebar.querySelector('[data-view="overview"]').setAttribute('aria-current', 'page');
  refreshTitle();
  on(sidebar, 'click', event => {
    const button = event.target.closest('[data-view]');
    if (button) selectView(button.dataset.view);
  });
  on(app, 'click', event => {
    const button = event.target.closest('[data-open-view]');
    if (button) selectView(button.dataset.openView);
  });
  on(app, 'pointerdown', event => {
    if (narrow.matches && !event.target.closest('#sidebarToggle')) setCollapsed(true);
  });
  const applyTheme = theme => {
    const next = theme === 'dark' ? 'dark' : 'light';
    document.documentElement.dataset.theme = next;
    document.querySelectorAll('[data-theme-choice]').forEach(button => {
      button.setAttribute('aria-pressed', String(button.dataset.themeChoice === next));
    });
    document.getElementById('themeToggle').setAttribute('aria-pressed', String(next === 'dark'));
    try { localStorage.setItem('carerover.theme', next); } catch { /* Private browsing: keep the in-memory selection. */ }
  };
  applyTheme(document.documentElement.dataset.theme);
  on(document.getElementById('themeToggle'), 'click', () => applyTheme(document.documentElement.dataset.theme === 'light' ? 'dark' : 'light'));
  document.querySelectorAll('[data-theme-choice]').forEach(button => on(button, 'click', () => applyTheme(button.dataset.themeChoice)));
  on(window, 'storage', event => { if (event.key === 'carerover.theme') applyTheme(event.newValue); });
  signal.addEventListener('abort', offLang, { once: true });
}
