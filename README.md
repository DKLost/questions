# questions
题库型的学习软件，采用FSRS算法，核心思路与anki相同。以填空题答题时间为依据判断对知识点的掌握程度。

计划实现的功能：

- [x] 跨题目答案绑定
- [x] 乱序答案池
- [ ] 添加父答案功能，并使乱序池只能在同层且具有相同子答案数量的答案间建立，从而使得父答案乱序可以改变填空对应子答案集。
- [ ] 去除标签功能，通过将一个问题归属多个分类来替代标签功能
- [ ] 将答案参与题目统计从单纯计数改为记录所有参与的题目的id，便于反向查找题目
- [ ] 添加答题时可在题目框显示同步显示答案功能，在作答时同步将输入显示在对应填空中
- [ ] 添加答案图层覆盖功能
- [ ] 添加题目类型功能
  - [ ] 通用型
  - [ ] 因果关系型：只呈现一个因果关系（如果一个知识点有对应的原因关系，则意味着这个知识点是可能被作为推论的，从而在重新组织知识架构时可以形成递推式而非混杂式的组织方式，一般不建议将因果关系型的题目参与到复习当中，而是将相关知识点重新组织绑定到通用型题目中再进行复习）
    - [ ] 题干区（函数定义区）

    - [ ] 答案区（变量定义区）
      - [ ] 分为两个区域，原因区和结果区，每个区域最多4个答案。






本项目FSRS算法实现主要来自该项目：https://github.com/open-spaced-repetition/fsrs4anki

感谢FSRS算法的创造者、实现者们。

