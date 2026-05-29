import rawGuide from "../../../HW_Doc/WLC_Complete_Guide.md?raw";
import { collectSearchText, parseGuide } from "../lib/guide-parser";

export const guideDocument = parseGuide(rawGuide);
export const guideSearchIndex = collectSearchText(guideDocument);

export const heroHighlights = [
  "整站只维护这一份教程正文，小白不用在多份文档里来回找",
  "内容顺序按“先看懂项目，再看代码，再看协议，再看排错”重新整理",
  "凡是额外根据源码补充的地方，都会保留明确标识，方便继续核对"
];

export const quickCards = [
  {
    title: "谁最适合先读",
    text: "第一次接手这个项目、之前没做过 Qi 发射端，或者看源码总觉得不知道该从哪下手的人。"
  },
  {
    title: "最稳的阅读顺序",
    text: "先看项目是什么、目录怎么读、怎么启动，再看 FreeRTOS 和主状态机，最后按协议阶段往后读。"
  },
  {
    title: "你现在看到的是什么",
    text: "这已经是新的统一教程站。旧 HTML 不再是主入口，后续只维护这一份正文和这一套导航。"
  }
];
