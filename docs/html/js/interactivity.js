// ===== Theme Toggle =====
(function() {
  const themeToggle = document.getElementById('theme-toggle');
  const html = document.documentElement;

  // Load saved theme
  const saved = localStorage.getItem('ptv-theme');
  if (saved === 'dark') {
    html.setAttribute('data-theme', 'dark');
    themeToggle.textContent = '☀️';
  } else {
    themeToggle.textContent = '🌙';
  }

  themeToggle.addEventListener('click', () => {
    const isDark = html.getAttribute('data-theme') === 'dark';
    html.setAttribute('data-theme', isDark ? 'light' : 'dark');
    themeToggle.textContent = isDark ? '🌙' : '☀️';
    localStorage.setItem('ptv-theme', isDark ? 'light' : 'dark');
    // Re-render Mermaid diagrams on theme switch
    if (typeof mermaid !== 'undefined') {
      mermaid.run({ nodes: document.querySelectorAll('.mermaid') });
    }
  });
})();

// ===== Floating TOC: Active Section Highlighting =====
(function() {
  const tocLinks = document.querySelectorAll('nav.toc a');
  const sections = [];

  tocLinks.forEach(link => {
    const href = link.getAttribute('href');
    if (href && href.startsWith('#')) {
      const el = document.getElementById(href.slice(1));
      if (el) sections.push({ el, link });
    }
  });

  function updateActive() {
    let current = '';
    for (const { el, link } of sections) {
      const rect = el.getBoundingClientRect();
      if (rect.top <= 100) {
        current = link;
      }
    }
    tocLinks.forEach(l => l.classList.remove('active'));
    if (current) current.classList.add('active');
  }

  window.addEventListener('scroll', updateActive);
  updateActive();
})();

// ===== Collapse/Expand All =====
(function() {
  const btn = document.getElementById('toggle-all');
  if (!btn) return;

  btn.addEventListener('click', () => {
    const details = document.querySelectorAll('main details');
    const allOpen = [...details].every(d => d.open);
    details.forEach(d => { d.open = !allOpen; });
    btn.textContent = allOpen ? '▶ Collapse All' : '▼ Expand All';
  });
})();

// ===== Search Filter =====
(function() {
  const search = document.getElementById('search-input');
  if (!search) return;

  search.addEventListener('input', () => {
    const q = search.value.toLowerCase().trim();
    const links = document.querySelectorAll('nav.toc a');

    links.forEach(link => {
      const text = link.textContent.toLowerCase();
      const parent = link.closest('li') || link;
      if (!q || text.includes(q)) {
        parent.style.display = '';
      } else {
        parent.style.display = 'none';
      }
    });
  });
})();

// ===== Collapsible Sections (details/summary) =====
(function() {
  // Wrap each h2 section in a details/summary for collapsibility
  // Only for sections numbered 3-15 (the detailed ones), not overview/intro
  const main = document.querySelector('main');
  if (!main) return;

  // Find h2s that have content sections we want collapsible
  const h2s = main.querySelectorAll('h2');
  h2s.forEach(h2 => {
    if (h2.id && /^[3-9]|1[0-5]/.test(h2.id)) {
      // Create a details wrapper
      const details = document.createElement('details');
      const summary = document.createElement('summary');
      summary.innerHTML = h2.innerHTML;
      details.appendChild(summary);

      // Move h2 and all following siblings until next h2 into details
      const content = document.createElement('div');
      content.className = 'content';

      let sibling = h2.nextElementSibling;
      const toMove = [];
      while (sibling && sibling.tagName !== 'H2') {
        toMove.push(sibling);
        sibling = sibling.nextElementSibling;
      }

      // Replace h2 with details
      h2.parentNode.replaceChild(details, h2);
      details.appendChild(content);
      toMove.forEach(el => content.appendChild(el));
    }
  });
})();

// ===== Tabbed Views =====
(function() {
  document.querySelectorAll('.tabs').forEach(tabs => {
    const buttons = tabs.querySelectorAll('.tab-btn');
    const contents = tabs.querySelectorAll('.tab-content');

    buttons.forEach(btn => {
      btn.addEventListener('click', () => {
        const target = btn.getAttribute('data-tab');
        buttons.forEach(b => b.classList.remove('active'));
        contents.forEach(c => c.classList.remove('active'));
        btn.classList.add('active');
        const content = tabs.querySelector(`.tab-content[data-tab="${target}"]`);
        if (content) content.classList.add('active');
      });
    });

    // Activate first tab
    if (buttons.length > 0) buttons[0].click();
  });
})();

// ===== Copy Button for Code Blocks =====
(function() {
  document.querySelectorAll('pre').forEach(pre => {
    const btn = document.createElement('button');
    btn.className = 'copy-btn';
    btn.textContent = 'Copy';
    pre.appendChild(btn);

    btn.addEventListener('click', async () => {
      const code = pre.querySelector('code');
      if (!code) return;
      try {
        await navigator.clipboard.writeText(code.textContent);
        btn.textContent = 'Copied!';
        setTimeout(() => { btn.textContent = 'Copy'; }, 2000);
      } catch {
        btn.textContent = 'Failed';
      }
    });
  });
})();

// ===== Language Labels on Code Blocks =====
(function() {
  document.querySelectorAll('pre code').forEach(code => {
    const cls = [...code.classList].find(c => c.startsWith('language-'));
    if (cls) {
      const label = document.createElement('span');
      label.className = 'lang-label';
      label.textContent = cls.replace('language-', '');
      code.parentElement.appendChild(label);
    }
  });
})();

// ===== Smooth Scroll for TOC Links =====
(function() {
  document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
      const id = a.getAttribute('href').slice(1);
      const el = document.getElementById(id);
      if (el) {
        e.preventDefault();
        el.scrollIntoView({ behavior: 'smooth', block: 'start' });
      }
    });
  });
})();
