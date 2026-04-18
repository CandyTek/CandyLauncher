# Git 中查找并恢复旧文件

这份文档记录如何从 Git 历史、reflog 和 `.git` 对象库里找回曾经提交过、后来被删除或在 squash/rebase 后丢失的文件。

示例文件使用本仓库曾经存在过的 `bkcode.txt`。它历史上出现过的路径包括：

- `unuseful/bkcode.txt`
- `deprecated/bkcode.txt`
- `bkcode.txt`

## 先确认当前工作区

恢复文件前先看工作区是否有未提交改动，避免覆盖正在编辑的文件。

```powershell
git status --short
```

如果 Git 报 `detected dubious ownership`，需要把当前仓库加入 safe directory：

```powershell
git config --global --add safe.directory I:/Github/CandyLauncher
```

## 查普通历史

先从所有分支的正常历史里查这个文件。

```powershell
git log --all --full-history --name-status -- "bkcode.txt"
git log --all --full-history --name-status -- "*bkcode.txt"
```

如果只查 `bkcode.txt` 没结果，说明文件可能在子目录里，例如 `deprecated/bkcode.txt`。

查看某个提交里该文件对应的 blob：

```powershell
git ls-tree -r <commit> -- deprecated/bkcode.txt
```

查看该提交对文件的改动概要：

```powershell
git show --stat --oneline <commit> -- deprecated/bkcode.txt
```

查看文件内容但不恢复：

```powershell
git show <commit>:deprecated/bkcode.txt
```

## 不要只找删除提交

删除文件的提交不是最后一版内容。删除提交只说明“从这一刻开始文件没了”。

正确做法是找删除提交之前，或者 reflog/squash 中间提交里，最后一个仍然包含该文件的版本。

例如：

```powershell
git log --all --full-history --format="%H %ad %s" --date=iso -- "deprecated/bkcode.txt"
```

如果最新记录是：

```text
8674c9b... D deprecated/bkcode.txt
79d1a60... M deprecated/bkcode.txt
```

那么 `8674c9b` 是删除提交，`79d1a60` 才是当前普通历史里删除前的文件版本。但如果之后做过 squash/rebase，`.git` 里可能还有更新的不可达版本。

## 查 reflog

reflog 会记录本地 HEAD 和分支曾经指向过的提交。被 rebase、squash 或 reset 掉的提交，通常还能在 reflog 里看到一段时间。

```powershell
git reflog --all --date=iso
```

可以筛选关键词：

```powershell
git reflog --all --date=iso | Select-String -Pattern "bkcode|rebase|squash|commit"
```

拿到可疑提交后，检查它是否包含文件：

```powershell
git ls-tree -r <commit> -- deprecated/bkcode.txt
```

如果有输出，说明该提交里存在这个文件，例如：

```text
100644 blob e759a9324e5f931b4b31d7a63776435c9c5e35b1    deprecated/bkcode.txt
```

这里的 `e759a932...` 就是文件内容对应的 blob ID。

## 查不可达对象

如果提交已经不在任何分支上，但还没有被 Git 垃圾回收，可以用 `git fsck` 查不可达对象：

```powershell
git fsck --unreachable --no-reflogs --full
```

输出里会有很多对象：

```text
unreachable commit <commit>
unreachable blob <blob>
unreachable tree <tree>
```

只看 blob 通常不知道路径，所以更可靠的方式是收集所有可疑 commit，再逐个检查是否包含目标文件。

PowerShell 示例：

```powershell
$hashes = @()
$hashes += git rev-list --all
$hashes += git reflog --all --format='%H'
$hashes += git fsck --unreachable --no-reflogs --full 2>$null | ForEach-Object {
	if ($_ -match '^unreachable commit ([0-9a-f]{40})') {
		$matches[1]
	}
}

$hashes = $hashes | Where-Object { $_ -match '^[0-9a-f]{40}$' } | Sort-Object -Unique

foreach ($h in $hashes) {
	foreach ($p in @('deprecated/bkcode.txt', 'unuseful/bkcode.txt', 'bkcode.txt')) {
		$line = git ls-tree -r $h -- $p 2>$null
		if ($LASTEXITCODE -eq 0 -and $line) {
			$blob = ($line -split '\s+')[2]
			$size = git cat-file -s $blob
			$date = git show -s --format='%cI' $h
			$subject = git show -s --format='%s' $h
			[pscustomobject]@{
				Date = $date
				Commit = $h
				Path = $p
				Blob = $blob
				Size = $size
				Subject = $subject
			}
		}
	}
} | Sort-Object Date -Descending | Format-Table -AutoSize
```

这个脚本会列出所有仍能找到目标文件的提交，并按提交时间倒序排列。

## 按 blob 归并版本

同一份文件内容可能出现在多个提交里。可以按 blob ID 归并，快速看有哪些不同版本：

```powershell
$rows | Group-Object Blob | ForEach-Object {
	$g = $_.Group | Sort-Object Date -Descending
	$first = $g[0]
	[pscustomobject]@{
		LatestDate = $first.Date
		LatestCommit = $first.Commit.Substring(0, 7)
		Path = $first.Path
		Blob = $first.Blob
		Size = $first.Size
		Count = $g.Count
		Subject = $first.Subject
	}
} | Sort-Object LatestDate -Descending | Format-Table -AutoSize
```

注意：这个片段依赖上一节脚本里的 `$rows` 变量。如果要使用它，需要把上一节脚本改成先保存结果：

```powershell
$rows = foreach ($h in $hashes) {
	# 上一节 foreach 的内容
}
```

## 恢复文件

### 从提交恢复到工作区

只恢复到工作区，不提交：

```powershell
git restore --source=<commit> --worktree -- deprecated/bkcode.txt
```

例如本次找回的最后一版：

```powershell
git restore --source=657a7c539f728192880f3dac12111ccd0616291b --worktree -- deprecated/bkcode.txt
```

恢复后验证文件内容对应的 blob：

```powershell
git hash-object deprecated/bkcode.txt
```

如果输出是下面这个值，说明内容就是这次找到的最后一版：

```text
e759a9324e5f931b4b31d7a63776435c9c5e35b1
```

### 从 blob 查看或恢复

只查看 blob 内容：

```powershell
git show e759a9324e5f931b4b31d7a63776435c9c5e35b1
```

如果只需要确认大小：

```powershell
git cat-file -s e759a9324e5f931b4b31d7a63776435c9c5e35b1
```

本次找到的 blob 大小是：

```text
100227
```

从 blob 写出文件也可以使用：

```powershell
git show e759a9324e5f931b4b31d7a63776435c9c5e35b1 > deprecated/bkcode.txt
```

不过更推荐从 commit 恢复，因为 commit 里保留了路径信息：

```powershell
git restore --source=<commit> --worktree -- deprecated/bkcode.txt
```

## 本次 bkcode.txt 的结论

当前普通历史中，删除前一版是：

```text
commit: 79d1a60d57901e195affd66defcb18ecf3300cd8
path:   deprecated/bkcode.txt
blob:   fb6369eb03f3cc3727476ad4a5f83d1c83014f28
```

但这不是最后一版。

从 reflog 和不可达提交里找到的最后内容是：

```text
commit: bd1c7a1a3fb24fb2acde156b9da3f9aed495011e
date:   2026-04-17T01:58:58+08:00
path:   deprecated/bkcode.txt
blob:   e759a9324e5f931b4b31d7a63776435c9c5e35b1
size:   100227
```

4 月 19 日 squash/rebase 的中间提交中，最后一个仍包含该文件的提交是：

```text
commit: 657a7c539f728192880f3dac12111ccd0616291b
blob:   e759a9324e5f931b4b31d7a63776435c9c5e35b1
```

恢复命令：

```powershell
git restore --source=657a7c539f728192880f3dac12111ccd0616291b --worktree -- deprecated/bkcode.txt
```

## 注意事项

- 不要运行 `git gc --prune=now`，否则不可达对象可能被清理，恢复机会会变小。
- 不要只看删除提交。最后一版内容通常在删除提交的父提交、reflog 或不可达 commit 中。
- 如果文件被 `.gitignore` 忽略，`git status --short` 可能看不到它。用下面命令确认：

```powershell
git status --short --ignored -- deprecated/bkcode.txt
```

如果显示：

```text
!! deprecated/bkcode.txt
```

说明文件存在，但被忽略。

如果需要重新纳入版本控制：

```powershell
git add -f deprecated/bkcode.txt
```

