const issuedSection = document.getElementById("sectionIssued");
const logsSection = document.getElementById("sectionLogs");
const insideSection = document.getElementById("sectionInside");

document.getElementById("tabIssued").addEventListener("click", () => showSection("issued"));
document.getElementById("tabLogs").addEventListener("click", () => showSection("logs"));
document.getElementById("tabInside").addEventListener("click", () => showSection("inside"));

function showSection(tab) {
  issuedSection.classList.add("d-none");
  logsSection.classList.add("d-none");
  insideSection.classList.add("d-none");

  document.getElementById("tabIssued").classList.remove("active");
  document.getElementById("tabLogs").classList.remove("active");
  document.getElementById("tabInside").classList.remove("active");

  if (tab === "issued") {
    issuedSection.classList.remove("d-none");
    document.getElementById("tabIssued").classList.add("active");
    loadIssued();
  } else if (tab === "logs") {
    logsSection.classList.remove("d-none");
    document.getElementById("tabLogs").classList.add("active");
    loadLogs();
  } else {
    insideSection.classList.remove("d-none");
    document.getElementById("tabInside").classList.add("active");
    loadInside();
  }
}

// ========== LOAD DATA ==========
async function loadIssued() {
  const res = await fetch("/issued");
  const data = await res.json();
  const tbody = document.querySelector("#issuedTable tbody");
  tbody.innerHTML = "";

  Object.values(data).forEach((rec) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${rec.student_name}</td>
      <td>${rec.book_title}</td>
      <td>${new Date(rec.borrow_date).toLocaleString()}</td>
      <td>${new Date(rec.due_date).toLocaleDateString()}</td>
      <td>${rec.status}</td>
      <td>${rec.status === "Issued" ? `<button class="btn btn-sm btn-success" onclick="returnBook('${rec.book_uid}')">Return</button>` : "—"}</td>
    `;
    tbody.appendChild(tr);
  });
}

async function loadLogs() {
  const res = await fetch("/logs");
  const data = await res.json();
  const tbody = document.querySelector("#logTable tbody");
  tbody.innerHTML = "";

  data.forEach((rec) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${rec.roll}</td>
      <td>${rec.name}</td>
      <td>${rec.dept}</td>
      <td>${rec.entry_time}</td>
      <td>${rec.exit_time || "—"}</td>
    `;
    tbody.appendChild(tr);
  });
}

async function loadInside() {
  const res = await fetch("/inside");
  const data = await res.json();
  const list = document.getElementById("insideList");
  list.innerHTML = "";

  if (data.length === 0) {
    list.innerHTML = `<li class="list-group-item">No students currently inside.</li>`;
    return;
  }

  data.forEach((rec) => {
    const li = document.createElement("li");
    li.className = "list-group-item";
    li.textContent = `${rec.name} (${rec.roll}) - ${rec.dept} | Entry: ${rec.entry_time}`;
    list.appendChild(li);
  });
}

async function returnBook(bookUid) {
  if (!confirm("Mark this book as returned?")) return;
  const res = await fetch("/return", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ book_uid: bookUid }),
  });
  const data = await res.json();
  alert(data.ok ? "Book marked as returned!" : "Error: " + data.error);
  loadIssued();
}

// Default load
showSection("issued");
