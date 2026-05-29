# tutorial-site

这是为 `HW_Doc/WLC_Complete_Guide.md` 准备的独立教程站骨架。

目标：

- 让统一教程拥有一个可继续演化的展示层
- 不影响当前固件工程和 Keil 工程目录
- 后续继续围绕唯一教程内容源演化展示层

## 结构说明

- `src/content/guide.ts`
  - 统一教程内容入口
  - 直接读取 `HW_Doc/WLC_Complete_Guide.md`
- `src/lib/guide-parser.ts`
  - 把 Markdown 按章节拆成页面可渲染的数据结构
- `src/App.tsx`
  - 教程页骨架
- `src/styles.css`
  - 当前教程页布局与风格

当前已经具备：

- 左侧章节导航
- 章节搜索
- 移动端目录展开收起
- “以下内容根据源码结构整理补充”高亮提示
- 旧 HTML 教程到新统一教程的映射卡片

## 运行方式

```bash
npm install
npm run dev
```

构建验证：

- 已在当前仓库环境中完成 `npm install`
- 已完成 `npm run build`
- 当前会输出到 `tutorial-site/dist/`

## animal-island-ui 接入说明

当前骨架已按官方 README 的方式预留：

```tsx
import { Button, Card } from "animal-island-ui";
import "animal-island-ui/style";
```

后续如果要进一步贴近该组件库的视觉语言，建议继续沿着下面三个方向增强：

1. 用它的更多基础组件替换当前 HTML 骨架部分。
2. 把章节导航、提示卡片、附录卡片收敛成可复用组件。
3. 继续保持 `WLC_Complete_Guide.md` 作为唯一正文来源，不在前端里重复维护另一份教程文本。

## 当前已知问题

当前构建虽然已经通过，但产物体积偏大，主要原因是组件库自带字体和样式资源较重。

后续如果要继续优化，建议优先做这三件事：

1. 按路由或章节拆包，而不是把所有正文和样式一次打进首页。
2. 评估是否保留组件库自带全部字体资源，必要时做裁剪。
3. 把长正文改成按章节懒加载，减少首屏脚本和样式体积。
