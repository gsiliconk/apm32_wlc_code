export type GuideBlock =
  | { type: "paragraph"; text: string }
  | { type: "subheading"; text: string }
  | { type: "supplement"; text: string }
  | { type: "quote"; lines: string[] }
  | { type: "bulletList"; items: string[] }
  | { type: "numberedList"; items: string[] }
  | { type: "code"; language: string; code: string };

export type GuideSection = {
  id: string;
  title: string;
  index: number;
  subheadings: Array<{
    id: string;
    text: string;
  }>;
  blocks: GuideBlock[];
};

export type GuideDocument = {
  title: string;
  intro: string[];
  sections: GuideSection[];
};

function slugify(input: string): string {
  return input
    .trim()
    .toLowerCase()
    .replace(/[`~!@#$%^&*()+=|{}[\]:;"'<>,.?/\\]/g, "")
    .replace(/\s+/g, "-");
}

function normalizeLine(line: string): string {
  return line.trimEnd();
}

function collectParagraph(lines: string[], start: number): [string, number] {
  const parts: string[] = [];
  let index = start;

  while (index < lines.length) {
    const line = lines[index].trim();
    if (
      !line ||
      line === "---" ||
      line.startsWith("### ") ||
      line.startsWith("> ") ||
      line.startsWith("- ") ||
      /^\d+\.\s/.test(line) ||
      line.startsWith("```")
    ) {
      break;
    }
    parts.push(line);
    index += 1;
  }

  return [parts.join(" "), index];
}

function parseSectionBlocks(source: string): GuideBlock[] {
  const lines = source.split("\n").map(normalizeLine);
  const blocks: GuideBlock[] = [];
  let index = 0;

  while (index < lines.length) {
    const raw = lines[index];
    const line = raw.trim();

    if (!line || line === "---") {
      index += 1;
      continue;
    }

    if (line.startsWith("```")) {
      const language = line.slice(3).trim();
      const codeLines: string[] = [];
      index += 1;
      while (index < lines.length && !lines[index].trim().startsWith("```")) {
        codeLines.push(lines[index]);
        index += 1;
      }
      if (index < lines.length) {
        index += 1;
      }
      blocks.push({
        type: "code",
        language,
        code: codeLines.join("\n").trimEnd()
      });
      continue;
    }

    if (line.startsWith("### ")) {
      blocks.push({
        type: "subheading",
        text: line.slice(4).trim()
      });
      index += 1;
      continue;
    }

    if (line === "以下内容根据源码结构整理补充") {
      blocks.push({
        type: "supplement",
        text: line
      });
      index += 1;
      continue;
    }

    if (line.startsWith("> ")) {
      const quoteLines: string[] = [];
      while (index < lines.length && lines[index].trim().startsWith("> ")) {
        quoteLines.push(lines[index].trim().slice(2).trim());
        index += 1;
      }
      blocks.push({
        type: "quote",
        lines: quoteLines
      });
      continue;
    }

    if (line.startsWith("- ")) {
      const items: string[] = [];
      while (index < lines.length && lines[index].trim().startsWith("- ")) {
        items.push(lines[index].trim().slice(2).trim());
        index += 1;
      }
      blocks.push({
        type: "bulletList",
        items
      });
      continue;
    }

    if (/^\d+\.\s/.test(line)) {
      const items: string[] = [];
      while (index < lines.length && /^\d+\.\s/.test(lines[index].trim())) {
        items.push(lines[index].trim().replace(/^\d+\.\s/, ""));
        index += 1;
      }
      blocks.push({
        type: "numberedList",
        items
      });
      continue;
    }

    const [paragraph, nextIndex] = collectParagraph(lines, index);
    if (paragraph) {
      blocks.push({
        type: "paragraph",
        text: paragraph
      });
    }
    index = nextIndex;
  }

  return blocks;
}

function collectSubheadings(sectionId: string, blocks: GuideBlock[]) {
  const counts = new Map<string, number>();

  return blocks.flatMap((block) => {
    if (block.type !== "subheading") {
      return [];
    }

    const base = slugify(`${sectionId}-${block.text}`);
    const currentCount = counts.get(base) ?? 0;
    counts.set(base, currentCount + 1);

    return [
      {
        id: currentCount === 0 ? base : `${base}-${currentCount + 1}`,
        text: block.text
      }
    ];
  });
}

export function parseGuide(markdown: string): GuideDocument {
  const normalized = markdown.replace(/\r\n/g, "\n");
  const lines = normalized.split("\n");
  const titleLine = lines.find((line) => line.startsWith("# "));
  const title = titleLine ? titleLine.slice(2).trim() : "WLC 项目教程";
  const sectionMatches = [...normalized.matchAll(/^##\s+(.+)$/gm)];
  const sections: GuideSection[] = [];
  const introEnd = sectionMatches.length > 0 ? sectionMatches[0].index ?? normalized.length : normalized.length;
  const introSource = normalized.slice(0, introEnd);
  const intro = introSource
    .split("\n")
    .map((line) => line.trim())
    .filter((line) => line && !line.startsWith("#") && line !== "---");

  for (let i = 0; i < sectionMatches.length; i += 1) {
    const current = sectionMatches[i];
    const next = sectionMatches[i + 1];
    const titleText = current[1].trim();
    const bodyStart = (current.index ?? 0) + current[0].length;
    const bodyEnd = next?.index ?? normalized.length;
    const body = normalized.slice(bodyStart, bodyEnd).trim();

    const id = slugify(titleText);
    const blocks = parseSectionBlocks(body);

    sections.push({
      id,
      title: titleText,
      index: i + 1,
      subheadings: collectSubheadings(id, blocks),
      blocks
    });
  }

  return {
    title,
    intro,
    sections
  };
}

export function collectSearchText(document: GuideDocument) {
  return document.sections.map((section) => {
    const body = section.blocks
      .map((block) => {
        if (block.type === "paragraph") return block.text;
        if (block.type === "subheading") return block.text;
        if (block.type === "supplement") return block.text;
        if (block.type === "quote") return block.lines.join(" ");
        if (block.type === "bulletList" || block.type === "numberedList") return block.items.join(" ");
        if (block.type === "code") return block.code;
        return "";
      })
      .join(" ");

    return {
      id: section.id,
      title: section.title,
      text: `${section.title} ${body}`.toLowerCase()
    };
  });
}
