import { useEffect, useMemo, useState, type CSSProperties } from "react";
import { Button, Card } from "animal-island-ui";
import { guideDocument, guideSearchIndex, heroHighlights, quickCards } from "./content/guide";

type InlineSegment = {
  text: string;
  code: boolean;
};

type HighlightPart = {
  text: string;
  match: boolean;
};

type GuideSection = (typeof guideDocument.sections)[number];
type GuideBlock = GuideSection["blocks"][number];

type SearchResult = {
  id: string;
  title: string;
  preview: string;
  targetId: string;
  targetLabel: string;
  sourceLabel: string;
};

type ResizeSide = "left" | null;

type SectionPrimer = {
  title: string;
  description: string;
  items: Array<{
    label: string;
    targetId: string;
  }>;
};

function splitInlineCode(text: string): InlineSegment[] {
  const parts = text.split("`");
  return parts.map((part, index) => ({
    text: part,
    code: index % 2 === 1
  }));
}

function escapeRegExp(text: string) {
  return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function splitHighlights(text: string, query: string): HighlightPart[] {
  if (!query.trim()) {
    return [{ text, match: false }];
  }

  const matcher = new RegExp(`(${escapeRegExp(query)})`, "ig");
  const parts = text.split(matcher);

  return parts
    .filter((part) => part.length > 0)
    .map((part) => ({
      text: part,
      match: part.toLowerCase() === query.toLowerCase()
    }));
}

function renderHighlightedText(text: string, query: string, keyPrefix: string) {
  return splitHighlights(text, query).map((part, index) =>
    part.match ? (
      <mark key={`${keyPrefix}-mark-${index}`} className="search-mark">
        {part.text}
      </mark>
    ) : (
      <span key={`${keyPrefix}-text-${index}`}>{part.text}</span>
    )
  );
}

function renderInline(text: string, query = "", keyPrefix = "inline") {
  return splitInlineCode(text).map((segment, index) =>
    segment.code ? (
      <code key={`${keyPrefix}-code-${index}`}>{segment.text}</code>
    ) : (
      <span key={`${keyPrefix}-span-${index}`}>
        {renderHighlightedText(segment.text, query, `${keyPrefix}-${index}`)}
      </span>
    )
  );
}

function buildSubheadingIds(sectionId: string, blocks: GuideSection["blocks"]) {
  const counts = new Map<string, number>();

  return blocks.map((block) => {
    if (block.type !== "subheading") {
      return undefined;
    }

    const base = `${sectionId}-${block.text}`
      .trim()
      .toLowerCase()
      .replace(/[`~!@#$%^&*()+=|{}[\]:;"'<>,.?/\\]/g, "")
      .replace(/\s+/g, "-");

    const currentCount = counts.get(base) ?? 0;
    counts.set(base, currentCount + 1);
    return currentCount === 0 ? base : `${base}-${currentCount + 1}`;
  });
}

function buildBlockAnchorId(sectionId: string, blockIndex: number) {
  return `${sectionId}-block-${blockIndex}`;
}

function collectBlockSearchTexts(block: GuideBlock) {
  if (block.type === "paragraph") return [block.text];
  if (block.type === "subheading") return [block.text];
  if (block.type === "supplement") return [block.text];
  if (block.type === "quote") return block.lines;
  if (block.type === "bulletList" || block.type === "numberedList") return block.items;
  if (block.type === "code") return [block.code];
  return [];
}

function buildPreviewSnippet(text: string, query: string) {
  const cleanText = text.replace(/\s+/g, " ").trim();
  if (!cleanText) {
    return "";
  }

  const normalizedQuery = query.trim().toLowerCase();
  const matchIndex = cleanText.toLowerCase().indexOf(normalizedQuery);
  if (matchIndex < 0) {
    return cleanText;
  }

  const start = Math.max(0, matchIndex - 28);
  const end = Math.min(cleanText.length, matchIndex + normalizedQuery.length + 54);
  const prefix = start > 0 ? "..." : "";
  const suffix = end < cleanText.length ? "..." : "";
  return `${prefix}${cleanText.slice(start, end).trim()}${suffix}`;
}

function getSidebarLabel(title: string) {
  const plain = title.replace(/^\d+\.\s*/, "").trim();

  const replacements: Array<[string, string]> = [
    ["先建立整体认知", "整体认知"],
    ["项目用途和支持能力", "用途能力"],
    ["目录结构怎么读", "目录结构"],
    ["项目入口和启动流程", "启动流程"],
    ["FreeRTOS 在这个工程里怎么工作", "FreeRTOS"],
    ["关键配置文件", "关键配置"],
    ["五层架构怎么理解", "五层架构"],
    ["`wlc_lib` 为什么这么重要", "wlc_lib"],
    ["BSP 层模块说明", "BSP 模块"],
    ["服务层模块说明", "服务模块"],
    ["应用层总状态机", "主状态机"],
    ["Qi 协议完整流程", "协议总流程"],
    ["数据包怎么读", "数据包"],
    ["构建、下载、运行", "构建运行"],
    ["新手快速开始", "快速开始"],
    ["常见错误排查", "错误排查"]
  ];

  return replacements.find(([source]) => plain === source)?.[1] ?? plain;
}

function buildSearchResult(section: GuideSection, query: string): SearchResult {
  const keyword = query.trim().toLowerCase();
  const subheadingIds = buildSubheadingIds(section.id, section.blocks);
  let currentTargetId = section.id;
  let currentTargetLabel = "定位到本章开头";
  let currentSourceLabel = `来自章节：${section.title}`;

  for (let index = 0; index < section.blocks.length; index += 1) {
    const block = section.blocks[index];

    if (block.type === "subheading") {
      currentTargetId = subheadingIds[index] ?? section.id;
      currentTargetLabel = `定位到小节：${block.text}`;
      currentSourceLabel = `来自小节：${block.text}`;
    }

    const hitText = collectBlockSearchTexts(block).find((text) => text.toLowerCase().includes(keyword));
    if (hitText) {
      return {
        id: section.id,
        title: section.title,
        preview: buildPreviewSnippet(hitText, query),
        targetId: block.type === "subheading" ? subheadingIds[index] ?? section.id : currentTargetId,
        targetLabel: block.type === "subheading" ? `定位到小节：${block.text}` : currentTargetLabel,
        sourceLabel: block.type === "subheading" ? `来自小节：${block.text}` : currentSourceLabel
      };
    }
  }

  return {
    id: section.id,
    title: section.title,
    preview: buildPreviewSnippet(section.title, query),
    targetId: section.id,
    targetLabel: "定位到本章开头",
    sourceLabel: `来自章节：${section.title}`
  };
}

function buildSectionPrimer(section: GuideSection): SectionPrimer | null {
  const findSection = (prefix: string) =>
    guideDocument.sections.find((item) => item.title.startsWith(prefix));

  const makeItems = (prefixes: string[]) =>
    prefixes.flatMap((prefix) => {
      const target = findSection(prefix);
      return target
        ? [
            {
              label: target.title,
              targetId: target.id
            }
          ]
        : [];
    });

  if (section.title.startsWith("7.")) {
    return {
      title: "新手提示",
      description: "如果你第一次看 RTOS 任务调度，建议先看完目录结构和启动流程，再回来读这一章。",
      items: makeItems(["5.", "6."])
    };
  }

  if (section.title.startsWith("9.") || section.title.startsWith("10.") || section.title.startsWith("11.") || section.title.startsWith("12.") || section.title.startsWith("13.")) {
    return {
      title: "建议先看",
      description: "这些章节已经开始进入工程分层和核心模块。如果前面的目录、入口、配置还没看顺，建议先补前置章节。",
      items: makeItems(["5.", "6.", "8."])
    };
  }

  if (section.title.startsWith("15.") || section.title.startsWith("16.") || section.title.startsWith("17.") || section.title.startsWith("18.") || section.title.startsWith("19.") || section.title.startsWith("20.")) {
    return {
      title: "阅读这章前，建议先补这几章",
      description: "协议阶段章节最容易越看越乱。先把主状态机和协议总流程看通，再回来看当前阶段会轻松很多。",
      items: makeItems(["13.", "14."])
    };
  }

  if (section.title.startsWith("21.") || section.title.startsWith("22.") || section.title.startsWith("23.") || section.title.startsWith("24.")) {
    return {
      title: "动手前建议先补",
      description: "这些章节偏向实际操作和排查。如果你还没建立起整体运行路径，先看入口、任务和协议主线会更稳。",
      items: makeItems(["6.", "7.", "13.", "14."])
    };
  }

  return null;
}

function getFaqCategoryLabel(text: string) {
  if (text.includes("没反应")) return "完全没反应";
  if (text.includes("卡在")) return "卡在某阶段";
  if (text.includes("掉回")) return "运行中掉回";
  if (text.includes("有文件")) return "模块像是没跑";
  return "代码理解问题";
}

function buildFaqSymptomGroups(section: GuideSection) {
  if (!section.title.startsWith("23.")) {
    return [];
  }

  const groups = new Map<string, Array<{ id: string; text: string }>>();
  section.subheadings.forEach((item) => {
    const category = getFaqCategoryLabel(item.text);
    const current = groups.get(category) ?? [];
    current.push(item);
    groups.set(category, current);
  });

  return Array.from(groups.entries()).map(([label, items]) => ({
    label,
    items
  }));
}

function findSectionIdByTitleIncludes(document: typeof guideDocument, keyword: string) {
  return document.sections.find((section) => section.title.includes(keyword))?.id ?? document.sections[0]?.id ?? "";
}

function App() {
  const [menuOpen, setMenuOpen] = useState(false);
  const [query, setQuery] = useState("");
  const [activeSectionId, setActiveSectionId] = useState(guideDocument.sections[0]?.id ?? "");
  const [readProgress, setReadProgress] = useState(0);
  const [openFaqSections, setOpenFaqSections] = useState<Record<string, boolean>>({});
  const [leftSidebarCollapsed, setLeftSidebarCollapsed] = useState(false);
  const [leftSidebarWidth, setLeftSidebarWidth] = useState(310);
  const [resizingSide, setResizingSide] = useState<ResizeSide>(null);
  const currentDocument = guideDocument;
  const currentSearchIndex = guideSearchIndex;
  const currentHeroHighlights = heroHighlights;
  const currentQuickCards = quickCards;
  const firstSection = guideDocument.sections[0];
  const quickStartGuideId = findSectionIdByTitleIncludes(guideDocument, "新手快速开始");
  const filteredSections = useMemo(() => {
    const keyword = query.trim().toLowerCase();
    if (!keyword) {
      return currentDocument.sections;
    }

    const visible = new Set(
      currentSearchIndex
        .filter((item) => item.text.includes(keyword))
        .map((item) => item.id)
    );

    return currentDocument.sections.filter((section) => visible.has(section.id));
  }, [currentDocument.sections, currentSearchIndex, query]);
  const sectionIds = useMemo(() => filteredSections.map((section) => section.id), [filteredSections]);
  const searchResults = useMemo(() => {
    const keyword = query.trim().toLowerCase();
    if (!keyword) {
      return [];
    }

    return filteredSections.map((section) => buildSearchResult(section, keyword));
  }, [filteredSections, query]);
  const activeSection = filteredSections.find((section) => section.id === activeSectionId);

  useEffect(() => {
    setActiveSectionId(guideDocument.sections[0]?.id ?? "");
    setQuery("");
    setMenuOpen(false);
    window.history.replaceState(null, "", "#top");
    window.scrollTo({ top: 0, behavior: "auto" });
  }, []);

  useEffect(() => {
    const visibleIds = new Set(sectionIds);
    if (!visibleIds.size) {
      return;
    }

    if (!visibleIds.has(activeSectionId)) {
      setActiveSectionId(sectionIds[0]);
    }
  }, [activeSectionId, sectionIds]);

  useEffect(() => {
    if (!resizingSide) {
      return;
    }

    const onPointerMove = (event: PointerEvent) => {
      setLeftSidebarWidth(Math.min(420, Math.max(240, event.clientX)));
    };

    const stopResizing = () => {
      setResizingSide(null);
    };

    window.addEventListener("pointermove", onPointerMove);
    window.addEventListener("pointerup", stopResizing);

    return () => {
      window.removeEventListener("pointermove", onPointerMove);
      window.removeEventListener("pointerup", stopResizing);
    };
  }, [resizingSide]);

  useEffect(() => {
    const onScroll = () => {
      const scrollTop = window.scrollY;
      const scrollHeight = document.documentElement.scrollHeight - window.innerHeight;
      const progress = scrollHeight > 0 ? Math.min(100, Math.max(0, (scrollTop / scrollHeight) * 100)) : 0;
      setReadProgress(progress);

      const candidates = filteredSections
        .map((section) => {
          const element = document.getElementById(section.id);
          if (!element) {
            return null;
          }
          const rect = element.getBoundingClientRect();
          return {
            id: section.id,
            top: rect.top
          };
        })
        .filter((item): item is { id: string; top: number } => item !== null);

      const current =
        candidates
          .filter((item) => item.top <= 180)
          .sort((a, b) => b.top - a.top)[0] ?? candidates[0];

      if (current && current.id !== activeSectionId) {
        setActiveSectionId(current.id);
      }
    };

    onScroll();
    window.addEventListener("scroll", onScroll, { passive: true });
    window.addEventListener("resize", onScroll);

    return () => {
      window.removeEventListener("scroll", onScroll);
      window.removeEventListener("resize", onScroll);
    };
  }, [activeSectionId, filteredSections]);

  function setFaqOpen(sectionId: string, open: boolean) {
    setOpenFaqSections((current) => ({
      ...current,
      [sectionId]: open
    }));
  }

  function isFaqSection(sectionTitle: string) {
    return sectionTitle.startsWith("23.");
  }

  function navigateToId(targetId: string, options?: { closeMenu?: boolean }) {
    if (options?.closeMenu) {
      setMenuOpen(false);
    }

    if (targetId === "top") {
      window.history.replaceState(null, "", "#top");
      window.scrollTo({ top: 0, behavior: "smooth" });
      return;
    }

    const target = document.getElementById(targetId);
    if (!target) {
      return;
    }

    const parentDetails = target.closest("details");
    if (parentDetails instanceof HTMLDetailsElement && !parentDetails.open) {
      parentDetails.open = true;
      const parentArticleId = target.closest("article")?.id;
      if (parentArticleId) {
        setFaqOpen(parentArticleId, true);
      }
    }

    const offset = window.innerWidth <= 720 ? 84 : 24;
    window.history.replaceState(null, "", `#${targetId}`);

    requestAnimationFrame(() => {
      const element = document.getElementById(targetId);
      if (!element) {
        return;
      }

      const top = window.scrollY + element.getBoundingClientRect().top - offset;
      window.scrollTo({ top: Math.max(0, top), behavior: "smooth" });
    });
  }

  return (
    <div
      className={`app-shell ${leftSidebarCollapsed ? "app-shell-left-collapsed" : ""}`}
      id="top"
      style={
        {
          "--left-sidebar-width": `${leftSidebarWidth}px`
        } as CSSProperties
      }
    >
      <div className="progress-bar" aria-hidden="true">
        <div className="progress-bar-fill" style={{ width: `${readProgress}%` }} />
      </div>
      <div className={`left-pane ${leftSidebarCollapsed ? "left-pane-collapsed" : ""}`}>
        <aside className={`sidebar ${menuOpen ? "sidebar-open" : ""}`}>
          <div className="pane-toolbar pane-toolbar-sidebar">
            {!leftSidebarCollapsed ? <div className="sidebar-badge">WLC 教程</div> : null}
            <button
              type="button"
              className="pane-toggle"
              onClick={() => setLeftSidebarCollapsed((collapsed) => !collapsed)}
            >
              {leftSidebarCollapsed ? "展开左栏" : "收起左栏"}
            </button>
          </div>
          {!leftSidebarCollapsed ? (
            <>
              <h1 className="sidebar-title">{guideDocument.title}</h1>
              <label className="sidebar-search">
                <span>在教程里找内容</span>
                <input
                  type="search"
                  value={query}
                  onChange={(event) => setQuery(event.target.value)}
                  placeholder="例如：Ping、功率调节、FreeRTOS"
                />
              </label>
              {searchResults.length > 0 ? (
                <div className="search-results-panel">
                  <p className="search-results-title">搜索结果</p>
                  <div className="search-results-list">
                    {searchResults.map((result) => (
                      <a
                        key={`${result.id}-${result.targetId}`}
                        href={`#${result.targetId}`}
                        className="search-result-card"
                        onClick={(event) => {
                          event.preventDefault();
                          navigateToId(result.targetId, { closeMenu: true });
                        }}
                      >
                        <strong>{renderHighlightedText(result.title, query, `${result.id}-result-title`)}</strong>
                        <small className="search-result-meta">
                          {renderHighlightedText(result.sourceLabel, query, `${result.id}-result-meta`)}
                        </small>
                        <span>{renderHighlightedText(result.preview, query, `${result.id}-result-preview`)}</span>
                        <em>{result.targetLabel}</em>
                      </a>
                    ))}
                  </div>
                </div>
              ) : query.trim() ? (
                <div className="search-results-panel search-results-empty">
                  <p className="search-results-title">搜索结果</p>
                  <p className="search-results-empty-text">没有找到匹配章节，请换个更直白的关键词试试。</p>
                </div>
              ) : null}
              <nav className="sidebar-nav" aria-label="教程章节导航">
                {filteredSections.map((section) => (
                  <a
                    key={section.id}
                    href={`#${section.id}`}
                    className={`sidebar-link ${section.id === activeSectionId ? "sidebar-link-active" : ""}`}
                    onClick={(event) => {
                      event.preventDefault();
                      navigateToId(section.id, { closeMenu: true });
                    }}
                  >
                    <span className="sidebar-index">{String(section.index).padStart(2, "0")}</span>
                    <span title={section.title}>{getSidebarLabel(section.title)}</span>
                  </a>
                ))}
              </nav>
            </>
          ) : (
            <div className="collapsed-pane-note" aria-hidden="true" />
          )}
        </aside>
        {!leftSidebarCollapsed ? (
          <button
            type="button"
            className={`pane-resizer pane-resizer-left ${resizingSide === "left" ? "pane-resizer-active" : ""}`}
            aria-label="调整左侧栏宽度"
            onPointerDown={(event) => {
              event.preventDefault();
              setResizingSide("left");
            }}
          />
        ) : null}
      </div>

      <main className="main-layout">
        <div className="top-actions">
          <button
            type="button"
            className="menu-toggle"
            onClick={() => setMenuOpen((open) => !open)}
          >
            {menuOpen ? "收起目录" : "展开目录"}
          </button>
          <p className="top-note">这份教程是按零基础接手项目的顺序重排的，建议按目录从前往后读一遍，再开始对代码。</p>
        </div>

        <header className="hero">
          <div className="hero-copy">
            <span className="hero-chip">WLC 教程入口</span>
            <h2>{currentDocument.title}</h2>
            <p>这份教程先帮你建立项目整体认知，再带你回到源码里看入口、任务、协议流程和排错路径。</p>
            <div className="hero-actions">
              <a
                href={`#${firstSection?.id ?? ""}`}
                className="hero-link"
                onClick={(event) => {
                  event.preventDefault();
                  if (firstSection?.id) {
                    navigateToId(firstSection.id);
                  }
                }}
              >
                <Button>开始阅读</Button>
              </a>
              <a href={`#${quickStartGuideId || firstSection?.id || ""}`} className="hero-link">
                <Button>直达新手快速开始</Button>
              </a>
            </div>
          </div>
          <Card>
            <div className="hero-card">
              <h3>这版骨架先解决三件事</h3>
              <ul>
                {currentHeroHighlights.map((item) => (
                  <li key={item}>{item}</li>
                ))}
              </ul>
            </div>
          </Card>
        </header>

        <section className="quick-grid" aria-label="快速说明">
          {currentQuickCards.map((card) => (
            <Card key={card.title}>
              <div className="quick-card">
                <h3>{card.title}</h3>
                <p>{card.text}</p>
              </div>
            </Card>
          ))}
        </section>

        <section className="intro-panel" aria-labelledby="intro-heading">
          <div className="section-head">
            <span className="section-kicker">阅读前</span>
            <h2 id="intro-heading">统一教程导读</h2>
          </div>
          <Card>
            <div className="intro-copy">
              {currentDocument.intro.map((paragraph, index) => (
                <p key={`${paragraph}-${index}`}>{renderInline(paragraph, query, `intro-${index}`)}</p>
              ))}
            </div>
          </Card>
        </section>

        <div className="content-layout">
          <section className="article" aria-label="教程正文">
            {filteredSections.map((section) => (
              <article key={section.id} id={section.id} className="article-section">
                {(() => {
                  const sectionIndex = filteredSections.findIndex((item) => item.id === section.id);
                  const previousSection = sectionIndex > 0 ? filteredSections[sectionIndex - 1] : undefined;
                  const nextSection =
                    sectionIndex >= 0 && sectionIndex < filteredSections.length - 1
                      ? filteredSections[sectionIndex + 1]
                      : undefined;
                  const blockSubheadingIds = buildSubheadingIds(section.id, section.blocks);
                  const sectionPrimer = buildSectionPrimer(section);
                  const faqSymptomGroups = buildFaqSymptomGroups(section);

                  return (
                    <>
                <div className="section-head">
                  <span className="section-kicker">章节 {section.index}</span>
                  <h2>{section.title}</h2>
                </div>
                {sectionPrimer ? (
                  <div className="section-primer">
                    <div className="section-primer-copy">
                      <strong>{sectionPrimer.title}</strong>
                      <p>{sectionPrimer.description}</p>
                    </div>
                    <div className="section-primer-links">
                      {sectionPrimer.items.map((item) => (
                        <a
                          key={item.targetId}
                          href={`#${item.targetId}`}
                          className="section-primer-link"
                          onClick={(event) => {
                            event.preventDefault();
                            navigateToId(item.targetId);
                          }}
                        >
                          {item.label}
                        </a>
                      ))}
                    </div>
                  </div>
                ) : null}
                <div className="section-body">
                  {isFaqSection(section.title) ? (
                    <details
                      className="faq-item"
                      open={openFaqSections[section.id] ?? false}
                      onToggle={(event) =>
                        setFaqOpen(section.id, (event.currentTarget as HTMLDetailsElement).open)
                      }
                    >
                      <summary className="faq-summary">
                        <span>{section.title}</span>
                        <span className="faq-indicator">{openFaqSections[section.id] ? "收起" : "展开"}</span>
                      </summary>
                      <div className="faq-content">
                        {faqSymptomGroups.length > 0 ? (
                          <div className="faq-symptom-finder">
                            <div className="faq-symptom-head">
                              <strong>按症状找问题</strong>
                              <p>如果你现在不知道从哪条异常开始查，可以先按现场现象选一个入口。</p>
                            </div>
                            <div className="faq-symptom-groups">
                              {faqSymptomGroups.map((group) => (
                                <div key={group.label} className="faq-symptom-group">
                                  <span className="faq-symptom-label">{group.label}</span>
                                  <div className="faq-symptom-links">
                                    {group.items.map((item) => (
                                      <a
                                        key={item.id}
                                        href={`#${item.id}`}
                                        className="faq-symptom-link"
                                        onClick={(event) => {
                                          event.preventDefault();
                                          navigateToId(item.id);
                                        }}
                                      >
                                        {item.text}
                                      </a>
                                    ))}
                                  </div>
                                </div>
                              ))}
                            </div>
                          </div>
                        ) : null}
                        {section.blocks.map((block, index) => {
                          if (block.type === "paragraph") {
                            return (
                              <p key={index} id={buildBlockAnchorId(section.id, index)}>
                                {renderInline(block.text, query, `${section.id}-faq-p-${index}`)}
                              </p>
                            );
                          }
                          if (block.type === "subheading") {
                            const subheadingId = blockSubheadingIds[index];
                            return (
                              <h3 key={index} id={subheadingId} className="section-subheading">
                                {block.text}
                              </h3>
                            );
                          }
                          if (block.type === "supplement") {
                            return (
                              <div key={index} id={buildBlockAnchorId(section.id, index)} className="supplement-callout">
                                <strong>{block.text}</strong>
                              </div>
                            );
                          }
                          if (block.type === "quote") {
                            return (
                              <blockquote key={index} id={buildBlockAnchorId(section.id, index)}>
                                {block.lines.map((line, lineIndex) => (
                                  <p key={`${line}-${lineIndex}`}>
                                    {renderInline(line, query, `${section.id}-faq-q-${index}-${lineIndex}`)}
                                  </p>
                                ))}
                              </blockquote>
                            );
                          }
                          if (block.type === "bulletList") {
                            return (
                              <ul key={index} id={buildBlockAnchorId(section.id, index)}>
                                {block.items.map((item, itemIndex) => (
                                  <li key={`${item}-${itemIndex}`}>
                                    {renderInline(item, query, `${section.id}-faq-b-${index}-${itemIndex}`)}
                                  </li>
                                ))}
                              </ul>
                            );
                          }
                          if (block.type === "numberedList") {
                            return (
                              <ol key={index} id={buildBlockAnchorId(section.id, index)}>
                                {block.items.map((item, itemIndex) => (
                                  <li key={`${item}-${itemIndex}`}>
                                    {renderInline(item, query, `${section.id}-faq-n-${index}-${itemIndex}`)}
                                  </li>
                                ))}
                              </ol>
                            );
                          }
                          return (
                            <pre key={index} id={buildBlockAnchorId(section.id, index)}>
                              <code>{block.code}</code>
                            </pre>
                          );
                        })}
                      </div>
                    </details>
                  ) : (
                    section.blocks.map((block, index) => {
                    if (block.type === "paragraph") {
                      return (
                        <p key={index} id={buildBlockAnchorId(section.id, index)}>
                          {renderInline(block.text, query, `${section.id}-p-${index}`)}
                        </p>
                      );
                    }
                    if (block.type === "subheading") {
                      const subheadingId = blockSubheadingIds[index];
                      return (
                        <h3 key={index} id={subheadingId} className="section-subheading">
                          {block.text}
                        </h3>
                      );
                    }
                    if (block.type === "supplement") {
                      return (
                        <div key={index} id={buildBlockAnchorId(section.id, index)} className="supplement-callout">
                          <strong>{block.text}</strong>
                        </div>
                      );
                    }
                    if (block.type === "quote") {
                      return (
                        <blockquote key={index} id={buildBlockAnchorId(section.id, index)}>
                          {block.lines.map((line, lineIndex) => (
                            <p key={`${line}-${lineIndex}`}>
                              {renderInline(line, query, `${section.id}-q-${index}-${lineIndex}`)}
                            </p>
                          ))}
                        </blockquote>
                      );
                    }
                    if (block.type === "bulletList") {
                      return (
                        <ul key={index} id={buildBlockAnchorId(section.id, index)}>
                          {block.items.map((item, itemIndex) => (
                            <li key={`${item}-${itemIndex}`}>
                              {renderInline(item, query, `${section.id}-b-${index}-${itemIndex}`)}
                            </li>
                          ))}
                        </ul>
                      );
                    }
                    if (block.type === "numberedList") {
                      return (
                        <ol key={index} id={buildBlockAnchorId(section.id, index)}>
                          {block.items.map((item, itemIndex) => (
                            <li key={`${item}-${itemIndex}`}>
                              {renderInline(item, query, `${section.id}-n-${index}-${itemIndex}`)}
                            </li>
                          ))}
                        </ol>
                      );
                    }
                    return (
                      <pre key={index} id={buildBlockAnchorId(section.id, index)}>
                        <code>{block.code}</code>
                      </pre>
                    );
                  })
                  )}
                </div>
                <div className="section-pagination">
                  {previousSection ? (
                    <a
                      href={`#${previousSection.id}`}
                      className="section-page-link section-page-link-prev"
                      onClick={(event) => {
                        event.preventDefault();
                        navigateToId(previousSection.id);
                      }}
                    >
                      <span className="section-page-label">上一篇</span>
                      <strong>{previousSection.title}</strong>
                    </a>
                  ) : (
                    <div className="section-page-link section-page-link-empty" />
                  )}
                  {nextSection ? (
                    <a
                      href={`#${nextSection.id}`}
                      className="section-page-link section-page-link-next"
                      onClick={(event) => {
                        event.preventDefault();
                        navigateToId(nextSection.id);
                      }}
                    >
                      <span className="section-page-label">下一篇</span>
                      <strong>{nextSection.title}</strong>
                    </a>
                  ) : (
                    <a
                      href="#top"
                      className="section-page-link section-page-link-next"
                      onClick={(event) => {
                        event.preventDefault();
                        navigateToId("top");
                      }}
                    >
                      <span className="section-page-label">阅读完成</span>
                      <strong>返回顶部</strong>
                    </a>
                  )}
                </div>
                    </>
                  );
                })()}
              </article>
            ))}
          </section>
        </div>
        <button
          type="button"
          className={`back-to-top back-to-top-floating ${readProgress > 8 ? "back-to-top-visible" : ""}`}
          onClick={() => navigateToId("top")}
        >
          返回顶部
        </button>
      </main>
    </div>
  );
}

export default App;
